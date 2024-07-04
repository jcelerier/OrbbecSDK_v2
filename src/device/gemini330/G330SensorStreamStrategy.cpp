
#include "G330SensorStreamStrategy.hpp"
#include "G330DepthAlgModeManager.hpp"
#include "exception/ObException.hpp"
#include <vector>
#include <sstream>
#include <memory>
#include <algorithm>

namespace libobsensor {

G330SensorStreamStrategy::G330SensorStreamStrategy(IDevice *owner) : DeviceComponentBase(owner) {}

G330SensorStreamStrategy::~G330SensorStreamStrategy() noexcept {}

void G330SensorStreamStrategy::markStreamStarted(const std::shared_ptr<const StreamProfile> &profile) {
    std::lock_guard<std::mutex> lock(startedStreamListMutex_);
    auto                        streamType = profile->getType();
    auto                        iter       = std::find_if(startedStreamList_.begin(), startedStreamList_.end(),
                                                          [streamType](const std::shared_ptr<const StreamProfile> &sp) { return sp->getType() == streamType; });
    if(iter != startedStreamList_.end()) {
        throw unsupported_operation_exception(utils::string::to_string() << "The " << streamType << " has already been started.");
    }

    startedStreamList_.push_back(profile);
}

void G330SensorStreamStrategy::markStreamStopped(const std::shared_ptr<const StreamProfile> &profile) {
    std::lock_guard<std::mutex> lock(startedStreamListMutex_);
    auto                        streamType = profile->getType();
    auto                        iter       = std::find_if(startedStreamList_.begin(), startedStreamList_.end(),
                                                          [streamType](const std::shared_ptr<const StreamProfile> &sp) { return sp->getType() == streamType; });
    if(iter == startedStreamList_.end()) {
        throw unsupported_operation_exception(utils::string::to_string() << "The " << streamType << " has not been started.");
    }
    startedStreamList_.erase(iter);
}

void G330SensorStreamStrategy::validateStartStream(const std::shared_ptr<const StreamProfile> &profile) {
    validateStartStream(std::vector<std::shared_ptr<const StreamProfile>>{ profile });
}

void G330SensorStreamStrategy::validateStartStream(const std::vector<std::shared_ptr<const StreamProfile>> &profiles) {
    {
        std::lock_guard<std::mutex> lock(startedStreamListMutex_);
        for(auto profile: profiles) {
            auto streamType = profile->getType();
            auto iter       = std::find_if(startedStreamList_.begin(), startedStreamList_.end(),
                                           [streamType](const std::shared_ptr<const StreamProfile> &sp) { return sp->getType() == streamType; });
            if(iter != startedStreamList_.end()) {
                throw unsupported_operation_exception(utils::string::to_string() << "The " << streamType << " has already been started.");
            }
        }
    }
    validateDepthAndIrStream(profiles);
    validatePreset(profiles);
}

void G330SensorStreamStrategy::validateDepthAndIrStream(const std::vector<std::shared_ptr<const StreamProfile>> &profiles) {
    std::lock_guard<std::mutex> lock(startedStreamListMutex_);
    auto                        tempStartedStreamList = startedStreamList_;
    for(auto &profile: profiles) {
        auto streamType = profile->getType();
        if(streamType != OB_STREAM_DEPTH && streamType != OB_STREAM_IR_LEFT && streamType != OB_STREAM_IR_RIGHT) {
            continue;
        }
        auto vsp  = profile->as<VideoStreamProfile>();
        auto iter = std::find_if(tempStartedStreamList.begin(), tempStartedStreamList.end(), [vsp](const std::shared_ptr<const StreamProfile> &sp) {
            auto cmpStreamType = sp->getType();
            if(cmpStreamType != OB_STREAM_DEPTH && cmpStreamType != OB_STREAM_IR_LEFT && cmpStreamType != OB_STREAM_IR_RIGHT) {
                return false;
            }
            auto cmpVsp = sp->as<VideoStreamProfile>();
            return cmpVsp->getWidth() != vsp->getWidth() && cmpVsp->getHeight() != vsp->getHeight() && cmpVsp->getFps() != vsp->getFps();
        });
        if(iter != tempStartedStreamList.end()) {
            throw unsupported_operation_exception(utils::string::to_string()
                                                  << "The depth/ir streams must have the same resolution and frame rate. " << profile);
        }
        tempStartedStreamList.push_back(profile);
    }
}

void G330SensorStreamStrategy::validatePreset(const std::vector<std::shared_ptr<const StreamProfile>> &profiles) {
    OBDepthAlgModeChecksum currentDepthMode;

    {
        auto owner               = getOwner();
        auto depthAlgModeManager = owner->getComponentT<G330DepthAlgModeManager>(OB_DEV_COMPONENT_DEPTH_ALG_MODE_MANAGER);
        currentDepthMode         = depthAlgModeManager->getCurrentDepthAlgModeChecksum();
    }

    for(auto profile: profiles) {
        auto        streamType  = profile->getType();
        const char *FactoryMode = "Factory Calib";
        if(strncmp(currentDepthMode.name, FactoryMode, strlen(FactoryMode)) == 0) {
            switch(streamType) {
            case OB_STREAM_IR_LEFT:
            case OB_STREAM_IR_RIGHT: {
                auto vsp    = profile->as<VideoStreamProfile>();
                auto width  = vsp->getWidth();
                auto height = vsp->getHeight();
                auto format = vsp->getFormat();
                if(!(((width == 1280 && height == 800) || (width == 640 && height == 400)) && (format == OB_FORMAT_Y12 || format == OB_FORMAT_Y16))) {
                    throw unsupported_operation_exception("Preset is in Factory Calib mode, only support IR streams at 1280x800@Y12/Y16 and 640x400@Y12/Y16");
                }
            } break;
            case OB_STREAM_DEPTH:
                throw unsupported_operation_exception("Preset is in Factory Calib mode, dose not support Depth stream");
                break;

            default:
                break;
            }
        }
    }
}

}  // namespace libobsensor