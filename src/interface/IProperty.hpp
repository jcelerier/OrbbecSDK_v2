#pragma once
#include <vector>
#include <memory>
#include "openobsdk/h/ObTypes.h"
#include "exception/ObException.hpp"
#include "property/HostProtocol.hpp"

namespace libobsensor {
typedef union {
    int32_t intValue;
    float   floatValue;
} OBPropertyValue;

typedef struct {
    OBPropertyValue cur;
    OBPropertyValue max;
    OBPropertyValue min;
    OBPropertyValue step;
    OBPropertyValue def;
} OBPropertyRange;

template <typename T> struct OBPropertyRangeT {
    T cur;
    T max;
    T min;
    T step;
    T def;
};

using get_data_callback = std::function<void(ob_data_tran_state state, ob_data_chunk *dataChunk)>;

class IPropertyPort {
public:
    virtual ~IPropertyPort() noexcept                                          = default;
    virtual void setPropertyValue(uint32_t propertyId, OBPropertyValue value)  = 0;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value) = 0;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range) = 0;
};

template <uint16_t CMD_VER> class IPropertyExtensionPort : public IPropertyPort {
public:
    virtual ~IPropertyExtensionPort() noexcept                                                                                                   = default;
    virtual void                              setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data)                            = 0;
    virtual const std::vector<uint8_t>       &getStructureData(uint32_t propertyId)                                                              = 0;
    virtual void                              getCmdVersionProtoV11(uint32_t propertyId, uint16_t *version)                                      = 0;
    virtual void                              getRawData(uint32_t propertyId, get_data_callback callback, uint32_t transPacketSize)              = 0;
    virtual std::vector<uint8_t>              getStructureDataProtoV11(uint32_t propertyId)                                                      = 0;
    virtual std::vector<std::vector<uint8_t>> getStructureDataListProtoV11(uint32_t propertyId, uint32_t tran_packet_size = 3 * FLASH_PAGE_SIZE) = 0;
};

enum PropertyAccessType {
    PROP_ACCESS_USER     = 1,  // User access(by sdk user api)
    PROP_ACCESS_INTERNAL = 2,  // Internal access (by sdk or other internal modules)
    PROP_ACCESS_ANY      = 3,  // Any access (user or internal)
};

class IPropertyAccessor {
public:
    virtual ~IPropertyAccessor() noexcept = default;

    virtual void registerProperty(uint32_t propertyId, OBPermissionType userPerms, OBPermissionType intPerms, std::shared_ptr<IPropertyPort> port)     = 0;
    virtual void registerProperty(uint32_t propertyId, const std::string &userPerms, const std::string &intPerms, std::shared_ptr<IPropertyPort> port) = 0;
    virtual void aliasProperty(uint32_t aliasId, uint32_t propertyId)                                                                                  = 0;

    virtual bool checkProperty(uint32_t propertyId, OBPermissionType permission, PropertyAccessType accessType = PROP_ACCESS_INTERNAL) const = 0;

    virtual void setPropertyValue(uint32_t propertyId, OBPropertyValue value, PropertyAccessType accessType = PROP_ACCESS_INTERNAL)  = 0;
    virtual void getPropertyValue(uint32_t propertyId, OBPropertyValue *value, PropertyAccessType accessType = PROP_ACCESS_INTERNAL) = 0;
    virtual void getPropertyRange(uint32_t propertyId, OBPropertyRange *range, PropertyAccessType accessType = PROP_ACCESS_INTERNAL) = 0;

    virtual void setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data, PropertyAccessType accessType = PROP_ACCESS_INTERNAL) = 0;
    virtual const std::vector<uint8_t> &getStructureData(uint32_t propertyId, PropertyAccessType accessType = PROP_ACCESS_INTERNAL)            = 0;

    template <typename T>
    typename std::enable_if<std::is_integral<T>::value || std::is_same<T, bool>::value, void>::type
    setPropertyValueT(uint32_t propertyId, const T &value, PropertyAccessType accessType = PROP_ACCESS_INTERNAL) {
        OBPropertyValue obValue;
        obValue.intValue = static_cast<int32_t>(value);
        setPropertyValue(propertyId, obValue, accessType);
    }

    template <typename T>
    typename std::enable_if<std::is_same<T, float>::value, void>::type setPropertyValueT(uint32_t propertyId, const T &value,
                                                                                         PropertyAccessType accessType = PROP_ACCESS_INTERNAL) {
        OBPropertyValue obValue;
        obValue.floatValue = static_cast<float>(value);
        setPropertyValue(propertyId, obValue, accessType);
    }

    template <typename T>
    typename std::enable_if<std::is_integral<T>::value || std::is_same<T, bool>::value, T>::type
    getPropertyValueT(uint32_t propertyId, PropertyAccessType accessType = PROP_ACCESS_INTERNAL) {
        OBPropertyValue obValue;
        getPropertyValue(propertyId, &obValue, accessType);
        return static_cast<T>(obValue.intValue);
    }

    template <typename T>
    typename std::enable_if<std::is_same<T, float>::value, T>::type getPropertyValueT(uint32_t           propertyId,
                                                                                      PropertyAccessType accessType = PROP_ACCESS_INTERNAL) {
        OBPropertyValue obValue;
        getPropertyValue(propertyId, &obValue, accessType);
        return static_cast<T>(obValue.floatValue);
    }

    template <typename T>
    typename std::enable_if<std::is_integral<T>::value || std::is_same<T, bool>::value, OBPropertyRangeT<T>>::type
    getPropertyRangeT(uint32_t propertyId, PropertyAccessType accessType = PROP_ACCESS_INTERNAL) {
        OBPropertyRangeT<T> rangeT;
        OBPropertyRange     range;
        getPropertyRange(propertyId, (OBPropertyRange *)&range, accessType);

        rangeT.cur  = range.cur.intValue;
        rangeT.max  = range.max.intValue;
        rangeT.min  = range.min.intValue;
        rangeT.step = range.step.intValue;
        rangeT.def  = range.def.intValue;

        return rangeT;
    }

    template <typename T>
    typename std::enable_if<std::is_same<T, float>::value, OBPropertyRangeT<T>>::type getPropertyRangeT(uint32_t           propertyId,
                                                                                                        PropertyAccessType accessType = PROP_ACCESS_INTERNAL) {
        OBPropertyRangeT<T> rangeT;
        OBPropertyRange     range;
        getPropertyRange(propertyId, (OBPropertyRange *)&range, accessType);

        rangeT.cur  = range.cur.floatValue;
        rangeT.max  = range.max.floatValue;
        rangeT.min  = range.min.floatValue;
        rangeT.step = range.step.floatValue;
        rangeT.def  = range.def.floatValue;

        return rangeT;
    }

    template <typename T> void setStructureDataT(uint32_t propertyId, const T &data, PropertyAccessType accessType = PROP_ACCESS_INTERNAL) {
        std::vector<uint8_t> vec(sizeof(T));
        std::memcpy(vec.data(), &data, sizeof(T));
        setStructureData(propertyId, vec, accessType);
    }

    template <typename T> T getStructureDataT(uint32_t propertyId, PropertyAccessType accessType = PROP_ACCESS_INTERNAL) {
        std::vector<uint8_t> vec = getStructureData(propertyId, accessType);
        T                    data;
        if(vec.size() != sizeof(T)) {
            LOG_WARN("Firmware data size is not match with property type");
        }
        std::memcpy(&data, vec.data(), std::min(vec.size(), sizeof(T)));
        return data;
    }
};

}  // namespace libobsensor