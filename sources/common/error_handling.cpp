#include "error_handling.h"
#include <cuda.h>
#include "spdlog/spdlog.h"
#include "ipc.pb.h"

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

int handleIpcError(ipc::Status status, const char* file, int line, const char* func, const char* msg) {
    if (status != ipc::ST_SUCCESS) {
        const char* errorName = statusToStr(status);
        spdlog::error("IPC Error: [{}] | File: {} | Line: {} | Function: {} | Message: {}", errorName, file, line, func, msg ? msg : "NONE");
        return static_cast<int>(status);
    }

    return EC_SUCCESS;
}

int handleCUDAError(CUresult status, const char* file, int line, const char* func, const char* msg) {
    if (status != CUDA_SUCCESS) {
        const char* errorName = "Unknown CUDA Error";
        const char* errorString = "No additional information";

        cuGetErrorName(status, &errorName);
        cuGetErrorString(status, &errorString);

        spdlog::error("CUDA Error: [{} - {}] | Description: {} | File: {} | Line: {} | Function: {} | Additional Info: {}",
            errorName, (int)status, errorString, file, line, func, msg ? msg : "NONE");

        return static_cast<int>(status);
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
    if constexpr (Type == ErrorType::IPC) {
        return handleIpcError(status, file, line, func, msg);
    } else if constexpr (Type == ErrorType::CUDA) {
        return handleCUDAError(status, file, line, func, msg);
    } else if constexpr (Type == ErrorType::DEFAULT) {
        return handleRegularError(status, file, line, func, msg);
    } else {
        return EC_FAILURE;
    }
}

template int errorCheck<ErrorType::IPC, ipc::Status>(ipc::Status status, const char* file, int line, const char* func, const char* msg);
template int errorCheck<ErrorType::CUDA, CUresult>(CUresult status, const char* file, int line, const char* func, const char* msg);
template int errorCheck<ErrorType::DEFAULT, int>(int status, const char* file, int line, const char* func, const char* msg);
