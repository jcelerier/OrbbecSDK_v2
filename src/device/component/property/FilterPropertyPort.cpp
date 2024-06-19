#include "FilterPropertyPort.hpp"
#include "openobsdk/h/Property.h"
#include "shared/utils/Utils.hpp"

namespace libobsensor {

FilterPropertyPort::FilterPropertyPort(const std::shared_ptr<IFilter> &backend) : backend_(backend) {
}

void FilterPropertyPort::setPropertyValue(uint32_t propertyId, OBPropertyValue value) {
    switch (propertyId)
    {
    case OB_PROP_SDK_ACCEL_FRAME_TRANSFORMED_BOOL:
    case OB_PROP_SDK_GYRO_FRAME_TRANSFORMED_BOOL:
        if(backend_->getName() == "IMUCorrecter") {
            backend_->enable(static_cast<bool>(value.intValue));
        }
        break;
    
    default:
        break;
    }

}

void FilterPropertyPort::getPropertyValue(uint32_t propertyId, OBPropertyValue *value) {
    switch (propertyId)
    {
    case OB_PROP_SDK_ACCEL_FRAME_TRANSFORMED_BOOL:
    case OB_PROP_SDK_GYRO_FRAME_TRANSFORMED_BOOL:
        if(backend_->getName() == "IMUCorrecter") {
            value->intValue = static_cast<int32_t>(backend_->isEnabled());
        }
        break;
    
    default:
        break;
    }
}

void FilterPropertyPort::getPropertyRange(uint32_t propertyId, OBPropertyRange *range) {
    //TODO
    switch (propertyId)
    {
    case OB_PROP_SDK_ACCEL_FRAME_TRANSFORMED_BOOL:
    case OB_PROP_SDK_GYRO_FRAME_TRANSFORMED_BOOL:
        if(backend_->getName() == "IMUCorrecter") {
            range->cur.intValue = static_cast<int32_t>(backend_->isEnabled());
            range->def.intValue = 1;
            range->max.intValue = 1;
            range->min.intValue = 0;
            range->step.intValue = 1;
        }
        break;
    
    default:
        break;
    }
}

}  // namespace libobsensor