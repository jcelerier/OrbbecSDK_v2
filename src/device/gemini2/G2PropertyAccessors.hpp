#pragma once
#include "IProperty.hpp"
#include "IDevice.hpp"

namespace libobsensor {
class G2Disp2DepthPropertyAccessor : public IBasicPropertyAccessor {
public:
    explicit G2Disp2DepthPropertyAccessor(IDevice *owner);
    virtual ~G2Disp2DepthPropertyAccessor() noexcept = default;

    virtual void setPropertyValue(uint32_t propertyId, OBPropertyValue value) override;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) override;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) override;

private:
    void markOutputDisparityFrame(bool enable);

private:
    IDevice *owner_;

    bool hwDisparityToDepthEnabled_;
    bool swDisparityToDepthEnabled_;
};
}  // namespace libobsensor