#pragma once
#include "ISourcePort.hpp"
#include "ethernet/rtsp/ObRTSPClient.hpp"
#include "ethernet/rtsp/ObRTPSink.hpp"
#include "ethernet/rtsp/ObUsageEnvironment.hpp"
#include "ethernet/VendorNetDataPort.hpp"

#include <string>
#include <memory>
#include <thread>

namespace libobsensor {
struct RTSPStreamPortInfo : public NetSourcePortInfo {
    RTSPStreamPortInfo(std::string address, uint16_t port, uint16_t vendorPort, OBStreamType streamType, std::string mac = "unknown",
                       std::string serialNumber = "unknown", uint32_t pid = 0)
        : NetSourcePortInfo(SOURCE_PORT_NET_RTSP, address, port, mac, serialNumber, pid), vendorPort(vendorPort), streamType(streamType) {}

    virtual bool equal(std::shared_ptr<const SourcePortInfo> cmpInfo) const override {
        if(cmpInfo->portType != portType) {
            return false;
        }
        auto netCmpInfo = std::dynamic_pointer_cast<const RTSPStreamPortInfo>(cmpInfo);
        return (address == netCmpInfo->address) && (port == netCmpInfo->port) && (vendorPort == netCmpInfo->vendorPort)
               && (streamType == netCmpInfo->streamType);
    };

    uint16_t     vendorPort;
    OBStreamType streamType;
};

class RTSPStreamPort : public IVideoStreamPort {
public:
    RTSPStreamPort(std::shared_ptr<const RTSPStreamPortInfo> portInfo);
    virtual ~RTSPStreamPort() noexcept;

    virtual std::vector<std::shared_ptr<const VideoStreamProfile>>             getStreamProfileList() override;
    virtual void                                  startStream(std::shared_ptr<const VideoStreamProfile> profile, FrameCallbackUnsafe callback) override;
    virtual void                                  stopStream(std::shared_ptr<const VideoStreamProfile> profile) override;
    virtual void                                  stopAllStream() override;
    virtual std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override;

private:
    void stopStream();
    void createClient(std::shared_ptr<const VideoStreamProfile> profile, FrameCallbackUnsafe callback);
    void closeClient();

private:
    std::shared_ptr<const RTSPStreamPortInfo> portInfo_;
    TaskScheduler                            *taskScheduler_;
    UsageEnvironment                         *live555Env_;
    std::thread                               eventLoopThread_;
    char                                      destroy_;
    bool                                      streamStarted_;

    ObRTSPClient                                *currentRtspClient_;
    std::shared_ptr<const VideoStreamProfile> currentStreamProfile_;
    std::vector<std::shared_ptr<const VideoStreamProfile>>                    streamProfileList_;
};
}  // namespace libobsensor