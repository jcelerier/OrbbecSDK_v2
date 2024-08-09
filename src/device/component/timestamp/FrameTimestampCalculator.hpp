#pragma once

#include "IFrame.hpp"
#include "DeviceComponentBase.hpp"
#include "GlobalTimestampFitter.hpp"

namespace libobsensor {
class GlobalTimestampCalculator : public IFrameTimestampCalculator, public DeviceComponentBase {
public:
    GlobalTimestampCalculator(IDevice *device, uint64_t deviceTimeFreq, uint64_t frameTimeFreq);

    virtual ~GlobalTimestampCalculator() = default;

    void calculate(std::shared_ptr<Frame> frame) override;
    void clear() override;

private:
    uint64_t                               deviceTimeFreq_;
    uint64_t                               frameTimeFreq_;
    std::shared_ptr<GlobalTimestampFitter> globalTimestampFitter_;
};

class FrameTimestampCalculatorBaseDeviceTime : public IFrameTimestampCalculator, public DeviceComponentBase {
public:
    FrameTimestampCalculatorBaseDeviceTime(IDevice *device, uint64_t deviceTimeFreq, uint64_t frameTimeFreq);

    virtual ~FrameTimestampCalculatorBaseDeviceTime() = default;

    void calculate(std::shared_ptr<Frame> frame) override;
    void clear() override;

protected:
    uint64_t calculate(uint64_t srcTimestamp);

private:
    uint64_t deviceTimeFreq_;
    uint64_t frameTimeFreq_;

    uint64_t prevSrcTsp_;
    uint64_t prevHostTsp_;
    uint64_t baseDevTime_;
};

class FrameTimestampCalculatorOverMetadata : public IFrameTimestampCalculator, public DeviceComponentBase {
public:
    FrameTimestampCalculatorOverMetadata(IDevice *device, OBFrameMetadataType metadataType, uint64_t clockFreq);

    virtual ~FrameTimestampCalculatorOverMetadata() = default;

    void calculate(std::shared_ptr<Frame> frame) override;
    void clear() override;

private:
    OBFrameMetadataType metadataType_;
    uint64_t            clockFreq_;
};

class FrameTimestampCalculatorOverUvcSCR : public IFrameTimestampCalculator, public DeviceComponentBase {
public:
    FrameTimestampCalculatorOverUvcSCR(IDevice *device, uint64_t clockFreq);

    virtual ~FrameTimestampCalculatorOverUvcSCR() = default;

    void calculate(std::shared_ptr<Frame> frame) override;
    void clear() override;

private:
    uint64_t clockFreq_;
};

}  // namespace libobsensor