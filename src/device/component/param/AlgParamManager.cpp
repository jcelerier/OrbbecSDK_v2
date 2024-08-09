#include "AlgParamManager.hpp"
#include "AlgParseHelper.hpp"
#include "property/InternalProperty.hpp"
#include "stream/StreamIntrinsicsManager.hpp"
#include "stream/StreamExtrinsicsManager.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "exception/ObException.hpp"
#include "publicfilters/IMUCorrector.hpp"

#include <vector>
#include <sstream>
#include <utility>

namespace libobsensor {

bool findBestMatchedD2CProfile(const std::vector<OBD2CProfile> &d2cProfileList, const std::shared_ptr<const VideoStreamProfile> &profile,
                               OBD2CProfile &result) {
    bool found = false;
    // match same resolution
    for(auto &d2cProfile: d2cProfileList) {
        // using software align profile
        if(d2cProfile.alignType != ALIGN_D2C_SW && d2cProfile.alignType != ALIGN_D2C_HW_SW_BOTH) {
            continue;
        }

        auto streamType = profile->getType();
        if((streamType == OB_STREAM_DEPTH || streamType == OB_STREAM_IR || streamType == OB_STREAM_IR_LEFT || streamType == OB_STREAM_IR_RIGHT)
           && static_cast<uint32_t>(d2cProfile.depthWidth) == profile->getWidth() && static_cast<uint32_t>(d2cProfile.depthWidth) == profile->getHeight()) {
            found  = true;
            result = d2cProfile;
            break;
        }
        else if(streamType == OB_STREAM_COLOR && static_cast<uint32_t>(d2cProfile.colorWidth) == profile->getWidth()
                && static_cast<uint32_t>(d2cProfile.colorHeight) == profile->getHeight()) {
            found  = true;
            result = d2cProfile;
            break;
        }
    }

    if(!found) {
        // match same ratio
        float ratio = (float)profile->getWidth() / profile->getHeight();
        for(auto &d2cProfile: d2cProfileList) {
            // using software align profile
            if(d2cProfile.alignType != ALIGN_D2C_SW && d2cProfile.alignType != ALIGN_D2C_HW_SW_BOTH) {
                continue;
            }

            auto streamType = profile->getType();
            if((streamType == OB_STREAM_DEPTH || streamType == OB_STREAM_IR || streamType == OB_STREAM_IR_LEFT || streamType == OB_STREAM_IR_RIGHT)
               && (float)d2cProfile.depthWidth / d2cProfile.depthHeight == ratio) {
                found  = true;
                result = d2cProfile;
                break;
            }
            else if(streamType == OB_STREAM_COLOR && (float)d2cProfile.colorWidth / d2cProfile.colorHeight == ratio) {
                found  = true;
                result = d2cProfile;
                break;
            }
        }
    }

    return found;
}

AlgParamManagerBase::AlgParamManagerBase(IDevice *owner) : DeviceComponentBase(owner) {}

void AlgParamManagerBase::bindStreamProfileParams(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) {
    bindExtrinsic(streamProfileList);
    bindIntrinsic(streamProfileList);
}

const std::vector<OBD2CProfile> &AlgParamManagerBase::getD2CProfileList() const {
    return d2cProfileList_;
}

const std::vector<OBCameraParam> &AlgParamManagerBase::getCalibrationCameraParamList() const {
    return calibrationCameraParamList_;
}

const OBIMUCalibrateParams &AlgParamManagerBase::getIMUCalibrationParam() const {
    return imuCalibParam_;
}

void AlgParamManagerBase::bindExtrinsic(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) {
    auto extrinsicMgr = StreamExtrinsicsManager::getInstance();
    for(auto &sp: streamProfileList) {
        auto basicSpIter = std::find_if(basicStreamProfileList_.begin(), basicStreamProfileList_.end(), [&](const auto &basicSp) {  //
            return basicSp->getType() == sp->getType();
        });
        if(basicSpIter == basicStreamProfileList_.end()) {
            throw libobsensor::unsupported_operation_exception("Can not find basic stream profile to bind extrinsic!");
        }
        extrinsicMgr->registerSameExtrinsics(sp, *basicSpIter);
    }
}

void AlgParamManagerBase::bindIntrinsic(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) {
    auto intrinsicMgr = StreamIntrinsicsManager::getInstance();
    bool mirrored     = false;

    {
        auto owner      = getOwner();
        auto propServer = owner->getPropertyServer();
        if(propServer->isPropertySupported(OB_PROP_DEPTH_MIRROR_MODULE_STATUS_BOOL, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
            mirrored = propServer->getPropertyValueT<bool>(OB_PROP_DEPTH_MIRROR_MODULE_STATUS_BOOL);
        }
    }

    for(const auto &sp: streamProfileList) {
        if(sp->is<AccelStreamProfile>()) {
            const auto &imuCalibParam = getIMUCalibrationParam();
            intrinsicMgr->registerAccelStreamIntrinsics(sp, imuCalibParam.singleIMUParams[0].acc);
        }
        else if(sp->is<GyroStreamProfile>()) {
            const auto &imuCalibParam = getIMUCalibrationParam();
            intrinsicMgr->registerGyroStreamIntrinsics(sp, imuCalibParam.singleIMUParams[0].gyro);
        }
        else {
            OBCameraIntrinsic  intrinsic  = { 0 };
            OBCameraDistortion distortion = { 0 };
            OBD2CProfile       d2cProfile{};
            auto               vsp = sp->as<VideoStreamProfile>();

            auto &d2cProfileList = getD2CProfileList();
            if(!findBestMatchedD2CProfile(d2cProfileList, vsp, d2cProfile)) {
                throw libobsensor::unsupported_operation_exception("Can not find matched camera param!");
            }

            const auto &calibrationCameraParamList = getCalibrationCameraParamList();
            const auto &param                      = calibrationCameraParamList.at(d2cProfile.paramIndex);
            auto        postProcessParam           = d2cProfile.postProcessParam;

            auto streamType = sp->getType();
            if(streamType == OB_STREAM_COLOR) {
                intrinsic  = param.rgbIntrinsic;
                distortion = param.rgbDistortion;
            }
            else {
                intrinsic  = param.depthIntrinsic;
                distortion = param.depthDistortion;
            }

            if(mirrored) {
                // mirror intrinsic
                intrinsic.cx  = (float)1.0 * intrinsic.width - intrinsic.cx - 1;
                distortion.p2 = -distortion.p2;
                std::swap(postProcessParam.alignLeft, postProcessParam.alignRight);
            }

            if(streamType == OB_STREAM_COLOR) {
                intrinsic.fx = postProcessParam.depthScale * intrinsic.fx;
                intrinsic.fy = postProcessParam.depthScale * intrinsic.fy;
                intrinsic.cx = postProcessParam.depthScale * intrinsic.cx + postProcessParam.alignLeft;
                intrinsic.cy = postProcessParam.depthScale * intrinsic.cy + postProcessParam.alignTop;
                intrinsic.width =
                    static_cast<int16_t>(postProcessParam.depthScale * intrinsic.width + postProcessParam.alignLeft + postProcessParam.alignRight);
                intrinsic.height =
                    static_cast<int16_t>(postProcessParam.depthScale * intrinsic.height + postProcessParam.alignTop + postProcessParam.alignBottom);
            }

            auto ratio = (float)vsp->getWidth() / (float)intrinsic.width;
            intrinsic.fx *= ratio;
            intrinsic.fy *= ratio;
            intrinsic.cx *= ratio;
            intrinsic.cy *= ratio;
            intrinsic.width  = static_cast<int16_t>(vsp->getWidth());
            intrinsic.height = static_cast<int16_t>((float)intrinsic.height * ratio);

            intrinsicMgr->registerVideoStreamIntrinsics(sp, intrinsic);
            intrinsicMgr->registerVideoStreamDistortion(sp, distortion);
        }
    }
}

DisparityAlgParamManagerBase::DisparityAlgParamManagerBase(IDevice *device) : AlgParamManagerBase(device) {}

void DisparityAlgParamManagerBase::bindStreamProfileParams(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) {
    AlgParamManagerBase::bindStreamProfileParams(streamProfileList);
    bindDisparityParam(streamProfileList);
}

const OBDisparityParam &DisparityAlgParamManagerBase::getDisparityParam() const {
    return disparityParam_;
}

void DisparityAlgParamManagerBase::bindDisparityParam(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) {
    auto dispParam    = getDisparityParam();
    auto intrinsicMgr = StreamIntrinsicsManager::getInstance();
    for(const auto &sp: streamProfileList) {
        if(!sp->is<DisparityBasedStreamProfile>()) {
            continue;
        }
        intrinsicMgr->registerDisparityBasedStreamDisparityParam(sp, dispParam);
    }
}

TOFDeviceCommandAlgParamManager::TOFDeviceCommandAlgParamManager(IDevice *owner) : AlgParamManagerBase(owner) {
    fetchParamFromDevice();
    registerBasicExtrinsics();
}

void TOFDeviceCommandAlgParamManager::fetchParamFromDevice() {
    std::vector<uint8_t> data;
    BEGIN_TRY_EXECUTE({
        auto owner      = getOwner();
        auto propServer = owner->getPropertyServer();
        propServer->getRawData(
            OB_RAW_DATA_ALIGN_CALIB_PARAM,
            [&](OBDataTranState state, OBDataChunk *dataChunk) {
                if(state == DATA_TRAN_STAT_TRANSFERRING) {
                    data.insert(data.end(), dataChunk->data, dataChunk->data + dataChunk->size);
                }
            },
            PROP_ACCESS_INTERNAL);
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_ERROR("Get align calibration params failed!");
        data.clear();
    })

    if(!data.empty()) {
        auto cameraParamList = AlgParseHelper::alignCalibParamParse(data.data(), static_cast<uint32_t>(data.size()));
        for(auto &cameraParam: cameraParamList) {
            OBCameraParam param;
            param.depthIntrinsic = cameraParam.depthIntrinsic;
            param.rgbIntrinsic   = cameraParam.rgbIntrinsic;
            memcpy(&param.depthDistortion, &cameraParam.depthDistortion, sizeof(param.depthDistortion));
            param.depthDistortion.model = OB_DISTORTION_BROWN_CONRADY_K6;
            memcpy(&param.rgbDistortion, &cameraParam.rgbDistortion, sizeof(param.rgbDistortion));
            param.rgbDistortion.model = OB_DISTORTION_BROWN_CONRADY;
            param.transform           = cameraParam.transform;
            param.isMirrored          = false;
            calibrationCameraParamList_.emplace_back(param);

            std::stringstream ss;
            ss << param;
            LOG_DEBUG("-{}", ss.str());
        }
    }

    // d2c align profile
    data.clear();
    BEGIN_TRY_EXECUTE({
        auto owner      = getOwner();
        auto propServer = owner->getPropertyServer();
        propServer->getRawData(
            OB_RAW_DATA_D2C_ALIGN_SUPPORT_PROFILE_LIST,
            [&](OBDataTranState state, OBDataChunk *dataChunk) {
                if(state == DATA_TRAN_STAT_TRANSFERRING) {
                    data.insert(data.end(), dataChunk->data, dataChunk->data + dataChunk->size);
                }
            },
            PROP_ACCESS_INTERNAL);
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_ERROR("Get align calibration params failed!");
        data.clear();
    })

    if(!data.empty()) {
        d2cProfileList_ = AlgParseHelper::d2cProfileInfoParse(data.data(), static_cast<uint32_t>(data.size()));
        // auto iter       = d2cProfileList_.begin();
        // while(iter != d2cProfileList_.end()) {
        //     if((*iter).alignType == ALIGN_D2C_HW_MODE) {
        //         iter = d2cProfileList_.erase(iter);
        //         continue;
        //     }
        //     iter++;
        // }
        LOG_DEBUG("Get depth to color profile list success! num={}", d2cProfileList_.size());
    }

    // imu param
    data.clear();
    BEGIN_TRY_EXECUTE({
        auto owner      = getOwner();
        auto propServer = owner->getPropertyServer();
        propServer->getRawData(
            OB_RAW_DATA_IMU_CALIB_PARAM,
            [&](OBDataTranState state, OBDataChunk *dataChunk) {
                if(state == DATA_TRAN_STAT_TRANSFERRING) {
                    data.insert(data.end(), dataChunk->data, dataChunk->data + dataChunk->size);
                }
            },
            PROP_ACCESS_INTERNAL);
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_ERROR("Get align calibration params failed!");
        data.clear();
    })

    if(!data.empty()) {
        auto realData  = data.data() + IMU_CALIBRATION_FILE_OFFSET;
        auto realSize  = static_cast<uint32_t>(data.size()) - IMU_CALIBRATION_FILE_OFFSET;
        imuCalibParam_ = IMUCorrector::parserIMUCalibParamRaw(realData, realSize);
        LOG_DEBUG("Get imu calibration params success!");
    }
    else {
        LOG_WARN("Get imu calibration param failed!load default param.");
        imuCalibParam_ = IMUCorrector::getDefaultImuCalibParam();
    }
}

void TOFDeviceCommandAlgParamManager::registerBasicExtrinsics() {
    auto extrinsicMgr            = StreamExtrinsicsManager::getInstance();
    auto depthBasicStreamProfile = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_DEPTH, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
    auto colorBasicStreamProfile = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_COLOR, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
    auto irBasicStreamProfile    = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_IR, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
    auto accelBasicStreamProfile = StreamProfileFactory::createAccelStreamProfile(OB_ACCEL_FS_2g, OB_SAMPLE_RATE_1_5625_HZ);
    auto gyroBasicStreamProfile  = StreamProfileFactory::createGyroStreamProfile(OB_GYRO_FS_16dps, OB_SAMPLE_RATE_1_5625_HZ);

    if(!calibrationCameraParamList_.empty()) {
        const auto &d2cExtrinsic = calibrationCameraParamList_.front().transform;
        extrinsicMgr->registerExtrinsics(depthBasicStreamProfile, colorBasicStreamProfile, d2cExtrinsic);
    }

    double imuExtr[16] = { 0 };
    memcpy(imuExtr, imuCalibParam_.singleIMUParams[0].imu_to_cam_extrinsics, sizeof(imuExtr));

    OBExtrinsic imu_to_depth;
    imu_to_depth.rot[0] = (float)imuExtr[0];
    imu_to_depth.rot[1] = (float)imuExtr[1];

    imu_to_depth.rot[2]   = (float)imuExtr[2];
    imu_to_depth.rot[3]   = (float)imuExtr[4];
    imu_to_depth.rot[4]   = (float)imuExtr[5];
    imu_to_depth.rot[5]   = (float)imuExtr[6];
    imu_to_depth.rot[6]   = (float)imuExtr[8];
    imu_to_depth.rot[7]   = (float)imuExtr[9];
    imu_to_depth.rot[8]   = (float)imuExtr[10];
    imu_to_depth.trans[0] = (float)imuExtr[3];
    imu_to_depth.trans[1] = (float)imuExtr[7];
    imu_to_depth.trans[2] = (float)imuExtr[11];
    extrinsicMgr->registerExtrinsics(accelBasicStreamProfile, depthBasicStreamProfile, imu_to_depth);
    extrinsicMgr->registerSameExtrinsics(gyroBasicStreamProfile, accelBasicStreamProfile);

    basicStreamProfileList_.emplace_back(depthBasicStreamProfile);
    basicStreamProfileList_.emplace_back(colorBasicStreamProfile);
    basicStreamProfileList_.emplace_back(irBasicStreamProfile);
    basicStreamProfileList_.emplace_back(accelBasicStreamProfile);
    basicStreamProfileList_.emplace_back(gyroBasicStreamProfile);
}

}  // namespace libobsensor