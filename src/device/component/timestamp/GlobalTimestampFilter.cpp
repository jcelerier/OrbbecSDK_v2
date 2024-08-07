#include "GlobalTimestampFilter.hpp"
#include "utils/Utils.hpp"
#include "logger/Logger.hpp"
#include "logger/LoggerInterval.hpp"
#include "InternalTypes.hpp"
#include "property/InternalProperty.hpp"
#include "environment/EnvConfig.hpp"

namespace libobsensor {
GlobalTimestampFilter::GlobalTimestampFilter(IDevice *owner) : DeviceComponentBase(owner), sampleLoopExit_(false), linearFuncParam_({ 0, 0, 0, 0 }) {
    auto envConfig = EnvConfig::getInstance();
    int  value     = 0;
    if(envConfig->getIntValue("Misc.GlobalTimestampFilterQueueSize", value) && value >= 4) {
        maxQueueSize_ = value;
    }
    value = 0;
    if(envConfig->getIntValue("Misc.GlobalTimestampFilterInterval", value) && value >= 100) {
        refreshIntervalMsec_ = value;
    }

    sampleThread_ = std::thread(&GlobalTimestampFilter::fittingLoop, this);

    std::unique_lock<std::mutex> lock(linearFuncParamMutex_);
    linearFuncParamCondVar_.wait_for(lock, std::chrono::milliseconds(1000));

    LOG_DEBUG("GlobalTimestampFilter created: maxQueueSize_={}, refreshIntervalMsec_={}", maxQueueSize_, refreshIntervalMsec_);
}

GlobalTimestampFilter::~GlobalTimestampFilter() {
    sampleLoopExit_ = true;
    sampleCondVar_.notify_one();
    if(sampleThread_.joinable()) {
        sampleThread_.join();
    }
}

LinearFuncParam GlobalTimestampFilter::getLinearFuncParam() {
    std::unique_lock<std::mutex> lock(linearFuncParamMutex_);
    return linearFuncParam_;
}

void GlobalTimestampFilter::reFitting() {
    std::unique_lock<std::mutex> lock(sampleMutex_);
    samplingQueue_.clear();
    sampleCondVar_.notify_one();
}

void GlobalTimestampFilter::pause() {
    sampleLoopExit_ = true;
    sampleCondVar_.notify_one();
    if(sampleThread_.joinable()) {
        sampleThread_.join();
    }
}

void GlobalTimestampFilter::resume() {
    sampleLoopExit_ = false;
    sampleThread_   = std::thread(&GlobalTimestampFilter::fittingLoop, this);
}

void GlobalTimestampFilter::fittingLoop() {
    std::unique_lock<std::mutex> lock(sampleMutex_);
    const int                    MAX_RETRY_COUNT = 5;

    int retryCount = 0;
    do {
        if(samplingQueue_.size() > maxQueueSize_) {
            samplingQueue_.pop_front();
        }

        uint64_t     sysTspUsec = 0;
        OBDeviceTime devTime;

        try {
            auto owner          = getOwner();
            auto propertyServer = owner->getPropertyServer();

            auto sysTsp1Usec = utils::getNowTimesUs();
            devTime          = propertyServer->getStructureDataT<OBDeviceTime>(OB_STRUCT_DEVICE_TIME);
            auto sysTsp2Usec = utils::getNowTimesUs();
            sysTspUsec       = (sysTsp2Usec + sysTsp1Usec) / 2;
            devTime.rtt      = sysTsp2Usec - sysTsp1Usec;
            if(devTime.rtt > 2000) {
                LOG_DEBUG("Get device time rtt is too large! rtt={}", devTime.rtt);
                throw std::runtime_error("RTT too large");
            }
            LOG_TRACE("sys={}, dev={}, rtt={}", sysTspUsec, devTime.time, devTime.rtt);
        }
        catch(...) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            retryCount++;
            continue;
        }

        // Successfully obtain timestamp, the number of retries is reset to zero
        retryCount = 0;

        // Clearing and refitting when the timestamp is out of order
        if(!samplingQueue_.empty() && (devTime.time < samplingQueue_.back().deviceTimestamp)) {
            samplingQueue_.clear();
        }

        samplingQueue_.push_back({ sysTspUsec, devTime.time });

        if(samplingQueue_.size() < 4) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        // Use the first set of data as offset to prevent overflow during calculation
        uint64_t offset_x = samplingQueue_.front().deviceTimestamp;
        uint64_t offset_y = samplingQueue_.front().systemTimestamp;
        double   Ex       = 0;
        double   Exx      = 0;
        double   Ey       = 0;
        double   Exy      = 0;
        auto     it       = samplingQueue_.begin();
        while(it != samplingQueue_.end()) {
            auto systemTimestamp = it->systemTimestamp - offset_y;
            auto deviceTimestamp = it->deviceTimestamp - offset_x;
            Ex += deviceTimestamp;
            Exx += deviceTimestamp * deviceTimestamp;
            Ey += systemTimestamp;
            Exy += deviceTimestamp * systemTimestamp;
            it++;
        }

        {
            std::unique_lock<std::mutex> linearFuncParamLock(linearFuncParamMutex_);
            // Linear regression to find a and b: y=ax+b
            linearFuncParam_.coefficientA = (Exy * samplingQueue_.size() - Ex * Ey) / (samplingQueue_.size() * Exx - Ex * Ex);
            linearFuncParam_.constantB  = (Exx * Ey - Exy * Ex) / (samplingQueue_.size() * Exx - Ex * Ex) + offset_y - linearFuncParam_.coefficientA * offset_x;
            linearFuncParam_.checkDataX = devTime.time;
            linearFuncParam_.checkDataY = sysTspUsec;

            // auto fixDevTsp = (double)devTime *linearFuncParam_.coefficientA + linearFuncParam_.constantB;
            // auto fixDiff   = fixDevTsp -sysTspUsec;
            // LOG_TRACE("a = {}, b = {}, fix={}, diff={}", linearFuncParam_.coefficientA, linearFuncParam_.constantB, fixDevTsp, fixDiff);

            LOG_DEBUG_INTVL("GlobalTimestampFilter update: coefficientA = {}, constantB = {}", linearFuncParam_.coefficientA, linearFuncParam_.constantB);
            linearFuncParamCondVar_.notify_all();
        }

        auto interval = refreshIntervalMsec_;
        if(samplingQueue_.size() >= 15) {
            interval *= 10;
        }
        sampleCondVar_.wait_for(lock, std::chrono::milliseconds(interval));

    } while(!sampleLoopExit_ && retryCount <= MAX_RETRY_COUNT);

    if(retryCount > MAX_RETRY_COUNT) {
        LOG_ERROR("GlobalTimestampFilter fittingLoop retry count exceed max retry count!");
    }

    LOG_DEBUG("GlobalTimestampFilter fittingLoop exit");
}

}  // namespace libobsensor