#pragma once
#include "libobsensor/h/ObTypes.h"
#include "InternalTypes.hpp"
#include <vector>

#define D2C_PARAMS_ITEM_SIZE 0xB0
#define IMU_CALIBRATION_FILE_OFFSET 60

namespace libobsensor {

class AlgParseHelper {
public:
    static std::vector<OBCameraParam> alignCalibParamParse(uint8_t *data, uint32_t size);
    static std::vector<OBD2CProfile>  d2cProfileInfoParse(uint8_t *data, uint32_t size);
};
}  // namespace libobsensor