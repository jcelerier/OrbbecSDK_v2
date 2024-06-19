#include "HostProtocol.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"

#include <sstream>

namespace libobsensor {
namespace protocol {

bool checkStatus(HpStatus stat, bool throwException) {
    std::string retMsg;
    switch(stat.statusCode) {
    case HP_STATUS_OK:
        break;
    case HP_STATUS_DEVICE_RESPONSE_WARNING:
        LOG_WARN("Request failed, device response with warning, errorCode: {0}, msg:{1}", stat.respErrorCode, stat.msg);
        return false;
    case HP_STATUS_DEVICE_RESPONSE_ERROR:
        retMsg = std::string("Request failed, device response with error, errorCode: ") + std::to_string(stat.respErrorCode) + ", msg: " + stat.msg;
        if(throwException) {
            throw io_exception(retMsg);
        }
        else {
            LOG_ERROR(retMsg);
            return false;
        }
        break;
    case HP_STATUS_DEVICE_RESPONSE_ERROR_UNKNOWN:
        if(throwException) {
            throw io_exception("Request failed, device response with unknown error!");
        }
        else {
            LOG_ERROR("Request failed, device response with unknown error!");
            return false;
        }
        break;

    default:
        retMsg = std::string("Request failed, statusCode: ") + std::to_string(stat.statusCode) + ", msg: " + stat.msg;
        if(throwException) {
            throw io_exception(retMsg);
        }
        else {
            LOG_ERROR(retMsg);
            return false;
        }
        break;
    }
    return true;
}

uint16_t getExpectedRespSize(HpOpCodes opcode) {
    uint16_t size = 64;
    if(opcode == OPCODE_GET_STRUCTURE_DATA || opcode == OPCODE_GET_STRUCTURE_DATA_V1_1 || opcode == OPCODE_HEARTBEAT_AND_STATE) {
        size = 512;
    }
    return size;
}

HpStatus validateResp(uint8_t *dataBuf, uint16_t dataSize, uint16_t expectedOpcode, uint16_t requestId) {
    HpStatus    retStatus;
    RespHeader *header = (RespHeader *)dataBuf;

    if(header->magic != HP_RESPONSE_MAGIC) {
        std::ostringstream ssMsg;
        ssMsg << "device response with bad magic " << std::hex << ", magic=0x" << header->magic << ", expectOpCode=0x" << HP_RESPONSE_MAGIC;
        retStatus.statusCode    = HP_STATUS_DEVICE_RESPONSE_BAD_MAGIC;
        retStatus.respErrorCode = HP_RESP_ERROR_UNKNOWN;
        retStatus.msg           = ssMsg.str();
        return retStatus;
    }

    if(header->requestId != requestId) {
        std::ostringstream ssMsg;
        ssMsg << "device response with inconsistent response requestId, cmdId=" << header->requestId << ", requestId=" << requestId;
        retStatus.statusCode    = HP_STATUS_DEVICE_RESPONSE_WRONG_ID;
        retStatus.respErrorCode = HP_RESP_ERROR_UNKNOWN;
        retStatus.msg           = ssMsg.str();
        return retStatus;
    }

    if(header->opcode != expectedOpcode) {
        std::ostringstream ssMsg;
        ssMsg << "device response with inconsistent opcode, opcode=" << header->opcode << ", expectedOpcode=" << expectedOpcode;
        retStatus.statusCode    = HP_STATUS_DEVICE_RESPONSE_WRONG_OPCODE;
        retStatus.respErrorCode = HP_RESP_ERROR_UNKNOWN;
        retStatus.msg           = ssMsg.str();
        return retStatus;
    }

    uint16_t respDataSize = header->sizeInHalfWords * 2 - sizeof(RespHeader::errorCode);
    if(respDataSize + HP_RESP_HEADER_SIZE > dataSize) {
        retStatus.statusCode    = HP_STATUS_DEVICE_RESPONSE_WRONG_DATA_SIZE;
        retStatus.respErrorCode = HP_RESP_ERROR_UNKNOWN;
        retStatus.msg           = "device response with wrong data size";
        return retStatus;
    }

    if(header->errorCode == HP_RESP_OK) {
        retStatus.statusCode    = HP_STATUS_OK;
        retStatus.respErrorCode = HP_RESP_OK;
        retStatus.msg           = "";
        return retStatus;
    }
    else if(header->errorCode == HP_RESP_ERROR_UNKNOWN) {
        retStatus.statusCode    = HP_STATUS_DEVICE_RESPONSE_ERROR_UNKNOWN;
        retStatus.respErrorCode = (HpRespErrorCode)header->errorCode;
        retStatus.msg           = "device response with unknown error";
        return retStatus;
    }

    std::string msg = respDataSize > 0 ? (char *)(dataBuf + sizeof(RespHeader)) : "";
    if(header->errorCode >= 0x8000 && header->errorCode <= 0xfffe) {
        retStatus.statusCode = HP_STATUS_DEVICE_RESPONSE_WARNING;
    }
    else {
        retStatus.statusCode = HP_STATUS_DEVICE_RESPONSE_ERROR;
    }
    retStatus.respErrorCode = (HpRespErrorCode)header->errorCode;
    retStatus.msg           = msg;
    return retStatus;
}

HpStatus execute(const std::shared_ptr<IVendorDataPort> &dataPort, uint8_t *reqData, uint16_t reqDataSize, uint8_t *respData, uint16_t *respDataSize) {
    HpStatusCode rc = HP_STATUS_OK;
    HpStatus     hpStatus;

    uint16_t requestId    = ((ReqHeader *)(reqData))->requestId;
    uint16_t opcode       = ((ReqHeader *)(reqData))->opcode;
    uint16_t nRetriesLeft = HP_NOT_READY_RETRIES;
    uint32_t exceptRecLen = getExpectedRespSize(static_cast<HpOpCodes>(opcode));

    while(nRetriesLeft-- > 0)  // loop until device is ready
    {
        rc = HP_STATUS_OK;
        try {
            *respDataSize = static_cast<uint16_t>(dataPort->sendAndReceive(reqData, static_cast<uint32_t>(reqDataSize), respData, exceptRecLen));
        }
        catch(...) {
            rc = HP_STATUS_CONTROL_TRANSFER_FAILED;
        }

        if(rc == HP_STATUS_OK) {
            hpStatus = validateResp(respData, *respDataSize, opcode, requestId);

            if(hpStatus.statusCode != HP_STATUS_DEVICE_RESPONSE_WRONG_ID && hpStatus.respErrorCode != HP_RESP_ERROR_DEVICE_BUSY) {
                break;
            }
        }
        else {
            hpStatus.statusCode    = rc;
            hpStatus.respErrorCode = HP_RESP_ERROR_UNKNOWN;
            hpStatus.msg           = "send control transfer failed!";
            break;
        }

        // Retry after delay
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return hpStatus;
}

uint16_t generateRequestId() {
    static uint16_t requestId = 0;
    requestId++;
    return requestId;
}

GetPropertyReq *initGetPropertyReq(uint8_t *dataBuf, uint32_t propertyId) {
    auto *req                   = reinterpret_cast<GetPropertyReq *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 2;
    req->header.opcode          = OPCODE_GET_PROPERTY;
    req->header.requestId       = generateRequestId();
    req->propertyId             = propertyId;
    return req;
}

SetPropertyReq *initSetPropertyReq(uint8_t *dataBuf, uint32_t propertyId, uint32_t value) {
    auto *req                   = reinterpret_cast<SetPropertyReq *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 4;
    req->header.opcode          = OPCODE_SET_PROPERTY;
    req->header.requestId       = generateRequestId();
    req->propertyId             = propertyId;
    req->value                  = value;
    return req;
}

GetStructureDataReq *initGetStructureDataReq(uint8_t *dataBuf, uint32_t propertyId) {
    auto *req                   = reinterpret_cast<GetStructureDataReq *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 2;
    req->header.opcode          = OPCODE_GET_STRUCTURE_DATA;

    req->header.requestId = generateRequestId();
    req->propertyId       = propertyId;
    return req;
}

SetStructureDataReq *initSetStructureDataReq(uint8_t *dataBuf, uint32_t propertyId, const uint8_t *data, uint16_t dataSize) {
    auto *req                   = reinterpret_cast<SetStructureDataReq *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 2 + (dataSize + 1) / 2;
    req->header.opcode          = OPCODE_SET_STRUCTURE_DATA;
    req->header.requestId       = generateRequestId();
    req->propertyId             = propertyId;
    memcpy(req->data, data, dataSize);
    return req;
}

GetPropertyResp *parseGetPropertyResp(uint8_t *dataBuf, uint16_t dataSize) {
    auto *resp = reinterpret_cast<GetPropertyResp *>(dataBuf);
    if(dataSize < sizeof(GetPropertyResp)) {
        throw io_exception("device response with wrong data size");
    }
    return resp;
}

SetPropertyResp *parseSetPropertyResp(uint8_t *dataBuf, uint16_t dataSize) {
    auto *resp = reinterpret_cast<SetPropertyResp *>(dataBuf);
    if(dataSize < sizeof(SetPropertyResp)) {
        throw io_exception("device response with wrong data size");
    }
    return resp;
}

GetStructureDataResp *parseGetStructureDataResp(uint8_t *dataBuf, uint16_t dataSize) {
    auto *resp = reinterpret_cast<GetStructureDataResp *>(dataBuf);
    if(dataSize < sizeof(GetStructureDataResp)) {
        throw io_exception("device response with wrong data size");
    }
    return resp;
}

uint16_t getStructureDataSize(const GetStructureDataResp *resp) {
    return resp->header.sizeInHalfWords * 2 - sizeof(RespHeader::errorCode);
}

SetStructureDataResp *parseSetStructureDataResp(uint8_t *dataBuf, uint16_t dataSize) {
    auto *resp = reinterpret_cast<SetStructureDataResp *>(dataBuf);
    if(dataSize < sizeof(SetStructureDataResp)) {
        throw io_exception("device response with wrong data size");
    }
    return resp;
}

GetPropertyReq *initGetCmdVersionReq(uint8_t *dataBuf, uint32_t propertyId) {
    auto *req                   = reinterpret_cast<GetPropertyReq *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 2;
    req->header.opcode          = OPCODE_GET_COMMAND_VERSION;
    req->header.requestId       = generateRequestId();
    req->propertyId             = propertyId;
    return req;
}

GetCmdVerDataResp *parseGetCmdVerDataResp(uint8_t *dataBuf, uint16_t dataSize) {
    auto *resp = reinterpret_cast<GetCmdVerDataResp *>(dataBuf);
    if(dataSize < sizeof(GetCmdVerDataResp)) {
        throw io_exception("device response with wrong data size");
    }
    return resp;
}

GetReadDataResp *parseGetReadDataResp(uint8_t *dataBuf, uint16_t dataSize) {
    auto *resp = reinterpret_cast<GetReadDataResp *>(dataBuf);
    if(dataSize < sizeof(GetReadDataResp)) {
        throw io_exception("device response with wrong data size");
    }
    return resp;
}

GetStructureDataV11Req *initGetStructureDataV11Req(uint8_t *dataBuf, uint32_t propertyId) {
    auto *req                   = reinterpret_cast<GetStructureDataV11Req *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 2;
    req->header.opcode          = OPCODE_GET_STRUCTURE_DATA_V1_1;

    req->header.requestId = generateRequestId();
    req->propertyId       = propertyId;
    return req;
}

GetStructureDataV11Resp *parseGetStructureDataV11Resp(uint8_t *dataBuf, uint16_t dataSize) {
    auto *resp = reinterpret_cast<GetStructureDataV11Resp *>(dataBuf);
    if(dataSize < sizeof(GetStructureDataV11Resp)) {
        throw io_exception("device response with wrong data size");
    }

    return resp;
}

GetStructureDataV11Req *initGetStructureDataListV11Req(uint8_t *dataBuf, uint32_t propertyId) {
    auto *req                   = reinterpret_cast<GetStructureDataV11Req *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 2;
    req->header.opcode          = OPCODE_INIT_READ_STRUCT_DATA_LIST;

    req->header.requestId = generateRequestId();
    req->propertyId       = propertyId;
    return req;
}

InitStructureDataListResp *parseInitStructureDataListResp(uint8_t *dataBuf, uint16_t dataSize) {
    auto *resp = reinterpret_cast<InitStructureDataListResp *>(dataBuf);
    if(dataSize < sizeof(InitStructureDataListResp)) {
        throw io_exception("device response with wrong data size");
    }

    return resp;
}

uint16_t getProtoV11StructureDataSize(const GetStructureDataV11Resp *resp) {
    return resp->header.sizeInHalfWords * 2 - sizeof(RespHeader) - 2;
}

GetPropertyReq *initStartGetStructureDataList(uint8_t *dataBuf, uint32_t propertyId) {
    auto *req                   = reinterpret_cast<GetPropertyReq *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 2;
    req->header.opcode          = OPCODE_INIT_READ_STRUCT_DATA_LIST;

    req->header.requestId = generateRequestId();
    req->propertyId       = propertyId;
    return req;
}

GetStructureDataListReq *initReadStructureDataList(uint8_t *dataBuf, uint32_t propertyId, uint32_t offset, uint32_t dataSize) {
    auto *req                   = reinterpret_cast<GetStructureDataListReq *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 6;
    req->header.opcode          = OPCODE_READ_STRUCT_DATA_LIST;

    req->header.requestId = generateRequestId();
    req->propertyId       = propertyId;

    req->offset = offset;
    req->size   = dataSize;
    return req;
}

GetPropertyReq *initFinishGetStructureDataList(uint8_t *dataBuf, uint32_t propertyId) {
    auto *req                   = reinterpret_cast<GetPropertyReq *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 2;
    req->header.opcode          = OPCODE_FINISH_READ_STRUCT_DATA_LIST;

    req->header.requestId = generateRequestId();
    req->propertyId       = propertyId;
    return req;
}

GetPropertyReq *initGetRawData(uint8_t *dataBuf, uint32_t propertyId, uint32_t cmd) {
    auto *req                   = reinterpret_cast<GetPropertyReq *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 0;
    switch(cmd) {
    case 0:
        req->header.opcode = OPCODE_INIT_READ_RAW_DATA;
        break;
    case 1:
        req->header.opcode = OPCODE_FINISH_READ_RAW_DATA;
        break;
    }

    req->header.requestId = generateRequestId();
    req->propertyId       = propertyId;
    return req;
}

ReadRawData *initReadRawData(uint8_t *dataBuf, uint32_t propertyId, uint32_t offset, uint32_t size) {
    auto *req                   = reinterpret_cast<ReadRawData *>(dataBuf);
    req->header.magic           = HP_REQUEST_MAGIC;
    req->header.sizeInHalfWords = 0;
    req->header.opcode          = OPCODE_READ_RAW_DATA;
    req->header.requestId       = generateRequestId();
    req->propertyId             = propertyId;

    req->offset = offset;
    req->size   = size;
    return req;
}

}  // namespace protocol
}  // namespace libobsensor