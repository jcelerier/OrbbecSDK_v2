#include "ImuStreamer.hpp"

#include "frame/Frame.hpp"
#include "frame/FrameFactory.hpp"
#include "stream/StreamProfile.hpp"
#include "logger/LoggerInterval.hpp"
#include "utils/Utils.hpp"
#include "publicfilters/IMUCorrector.hpp"

namespace libobsensor {
ImuStreamer::ImuStreamer(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend, const std::shared_ptr<IFilter> &corrector)
    : owner_(owner), backend_(backend), corrector_(corrector), running_(false), frameIndex_(0) {

    if(corrector_) {
        corrector_->setCallback([this](std::shared_ptr<Frame> frame) { outputFrame(frame); });
    }
    LOG_DEBUG("ImuStreamer created");
}

ImuStreamer::~ImuStreamer() noexcept {
    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        callbacks_.clear();
    }

    if(running_) {
        backend_->stopStream();
        if(corrector_) {
            corrector_->reset();
        }
        running_ = false;
    }
}

void ImuStreamer::start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) {
    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        callbacks_[sp] = callback;
        if(running_) {
            return;
        }
    }
    running_ = true;

    backend_->startStream([this](std::shared_ptr<Frame> frame) { ImuStreamer::parseIMUData(frame); });
}

void ImuStreamer::stop(std::shared_ptr<const StreamProfile> sp) {
    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        auto                        iter = callbacks_.find(sp);
        if(iter == callbacks_.end()) {
            throw invalid_value_exception("Stop stream failed, stream profile not found.");
        }

        callbacks_.erase(iter);
        if(!callbacks_.empty()) {
            return;
        }
    }

    backend_->stopStream();
    if(corrector_) {
        corrector_->reset();
    }
    running_ = false;
}

void ImuStreamer::parseIMUData(std::shared_ptr<Frame> frame) {
    auto         data     = frame->getData();
    OBImuHeader *header   = (OBImuHeader *)data;
    auto         dataSize = frame->getDataSize();

    if(header->reportId != 1) {
        LOG_WARN_INTVL("Imu header is invalid,drop imu package!");
        return;
    }

    const auto computeDataSize = sizeof(OBImuHeader) + sizeof(OBImuOriginData) * header->groupCount;
    if(dataSize < computeDataSize) {
        LOG_WARN_INTVL("Imu header is invalid, drop imu package!, invalid data size. dataSize={}, computeDataSize={}, groupCount={}", dataSize, computeDataSize,
                       header->groupCount);
        return;
    }

    const auto computeDataSizeP = sizeof(OBImuHeader) + header->groupLen * header->groupCount;
    if(dataSize < computeDataSizeP) {
        LOG_WARN_INTVL("Imu header is invalid, drop imu package!, invalid data size. dataSize={}, computeDataSizeP={}, groupCount={}", dataSize,
                       computeDataSizeP, header->groupCount);
        return;
    }

    std::shared_ptr<const AccelStreamProfile> accelStreamProfile;
    std::shared_ptr<const GyroStreamProfile>  gyroStreamProfile;
    for(const auto &iter: callbacks_) {
        if(iter.first->is<libobsensor::AccelStreamProfile>()) {
            accelStreamProfile = iter.first->as<libobsensor::AccelStreamProfile>();
        }
        else if(iter.first->is<libobsensor::GyroStreamProfile>()) {
            gyroStreamProfile = iter.first->as<libobsensor::GyroStreamProfile>();
        }
    }

    uint8_t *imuOrgData = (uint8_t *)data + sizeof(OBImuHeader);
    for(int groupIndex = 0; groupIndex < header->groupCount; groupIndex++) {
        auto frameSet = FrameFactory::createFrameSet();

        OBImuOriginData *imuData    = (OBImuOriginData *)((uint8_t *)imuOrgData + groupIndex * sizeof(OBImuOriginData));
        uint64_t         timestamp  = ((uint64_t)imuData->timestamp[0] | ((uint64_t)imuData->timestamp[1] << 32));
        auto             sysTspUs   = utils::getNowTimesUs();
        auto             frameIndex = frameIndex_++;

        if(accelStreamProfile) {
            auto              accelFrame     = FrameFactory::createFrameFromStreamProfile(accelStreamProfile);
            auto              accelFrameData = (AccelFrame::Data *)accelFrame->getData();
            auto              fs             = static_cast<uint8_t>(accelStreamProfile->getFullScaleRange());

            accelFrameData->value.x = IMUCorrector::calculateAccelGravity(static_cast<int16_t>(imuData->accelX), fs);
            accelFrameData->value.y = IMUCorrector::calculateAccelGravity(static_cast<int16_t>(imuData->accelY), fs);
            accelFrameData->value.z = IMUCorrector::calculateAccelGravity(static_cast<int16_t>(imuData->accelZ), fs);
            accelFrameData->temp    = IMUCorrector::calculateRegisterTemperature(imuData->temperature);

            accelFrame->setNumber(frameIndex);
            accelFrame->setTimeStampUsec(timestamp);
            accelFrame->setSystemTimeStampUsec(sysTspUs);
            frameSet->pushFrame(accelFrame);
        }

        if(gyroStreamProfile) {
            auto             gyroFrame     = FrameFactory::createFrameFromStreamProfile(gyroStreamProfile);
            auto             gyroFrameData = (GyroFrame::Data *)gyroFrame->getData();
            auto             fs            = static_cast<uint8_t>(gyroStreamProfile->getFullScaleRange());
            gyroFrameData->value.x         = IMUCorrector::calculateGyroDPS(static_cast<int16_t>(imuData->gyroX), fs);
            gyroFrameData->value.y         = IMUCorrector::calculateGyroDPS(static_cast<int16_t>(imuData->gyroY), fs);
            gyroFrameData->value.z         = IMUCorrector::calculateGyroDPS(static_cast<int16_t>(imuData->gyroZ), fs);
            gyroFrameData->temp            = IMUCorrector::calculateRegisterTemperature(imuData->temperature);

            gyroFrame->setNumber(frameIndex);
            gyroFrame->setTimeStampUsec(timestamp);
            gyroFrame->setSystemTimeStampUsec(sysTspUs);
            frameSet->pushFrame(gyroFrame);
        }

        if(corrector_ && corrector_->isEnabled()) {
            corrector_->pushFrame(frameSet);
        }
        else {
            outputFrame(frameSet);
        }
    }
}

void ImuStreamer::outputFrame(std::shared_ptr<Frame> frame) {
    if(!frame) {
        return;
    }

    std::lock_guard<std::mutex> lock(cbMtx_);
    for(auto &callback: callbacks_) {
        std::shared_ptr<const Frame> callbackFrame = frame;
        if(frame->is<FrameSet>()) {
            auto frameSet = frame->as<FrameSet>();
            callbackFrame = frameSet->getFrame(utils::mapStreamTypeToFrameType(callback.first->getType()));
            if(!callbackFrame) {
                continue;
            }
        }
        if(callbackFrame->getFormat() != callback.first->getFormat()) {
            continue;
        }
        callback.second(callbackFrame);
    }
}

IDevice *ImuStreamer::getOwner() const {
    return owner_;
}

}  // namespace libobsensor