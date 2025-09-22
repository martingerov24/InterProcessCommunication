#include "error_handling.h"
#include "spdlog/spdlog.h"
#include "ipc.pb.h"
#include "zmq.hpp"

static const char* statusToStr(ipc::Status s) {
    switch (s) {
    case ipc::ST_SUCCESS:                return "SUCCESS";
    case ipc::ST_ERROR_INVALID_INPUT:    return "ERROR_INVALID_INPUT";
    case ipc::ST_ERROR_DIV_BY_ZERO:      return "ERROR_DIV_BY_ZERO";
    case ipc::ST_ERROR_SUBSTR_NOT_FOUND: return "ERROR_SUBSTR_NOT_FOUND";
    case ipc::ST_ERROR_STRING_TOO_LONG:  return "ERROR_STRING_TOO_LONG";
    case ipc::ST_ERROR_INTERNAL:         return "ERROR_INTERNAL";
    case ipc::ST_NOT_FINISHED:           return "NOT_FINISHED";
    default: return "UNKNOWN";
    }
}

int handleIpcError(ipc::Status status) {
    if (status != ipc::ST_SUCCESS) {
        const char* errorName = statusToStr(status);
        if (status == ipc::ST_NOT_FINISHED) {
            printf("IPC Info: [%s]\n", errorName);
            spdlog::info("IPC Error: [{}]", errorName);
            return EC_SUCCESS;
        } else {
            printf("IPC Error: [%s]\n", errorName);
            spdlog::error("IPC Error: [{}]", errorName);
        }
        return static_cast<int>(status);
    }

    return EC_SUCCESS;
}

int handleZmqSendError(zmq::send_result_t status, const char* file, int line, const char* func, const char* msg) {
    if (status.has_value() == false) {
        spdlog::error("ZMQ Send Error: [Failed to send message] | File: {} | Line: {} | Function: {} | Message: {}", file, line, func, msg ? msg : "NONE");
        return EC_FAILURE;
    }

    return EC_SUCCESS;
}

int handleRegularError(int status, const char* file, int line, const char* func, const char* msg) {
    if (status != EC_SUCCESS) {
        spdlog::error("Regular Error: [{}] | File: {} | Line: {} | Function: {} | Message: {}", status, file, line, func, msg ? msg : "NONE");
        return status;
    }
    return EC_SUCCESS;
}

template<ErrorType Type, typename StatusType>
int errorCheck(StatusType status, const char* file, int line, const char* func, const char* msg) {
    if constexpr (Type == ErrorType::ZMQ_SEND) {
        return handleZmqSendError(status, file, line, func, msg);
    } else if constexpr (Type == ErrorType::IPC) {
        return handleIpcError(status);
    } else if constexpr (Type == ErrorType::DEFAULT) {
        return handleRegularError(status, file, line, func, msg);
    } else {
        return EC_FAILURE;
    }
}

template int errorCheck<ErrorType::ZMQ_SEND, zmq::send_result_t>(zmq::send_result_t status, const char* file, int line, const char* func, const char* msg);
template int errorCheck<ErrorType::IPC, ipc::Status>(ipc::Status status, const char* file, int line, const char* func, const char* msg);
template int errorCheck<ErrorType::DEFAULT, int>(int status, const char* file, int line, const char* func, const char* msg);
