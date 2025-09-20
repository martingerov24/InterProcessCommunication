#include "error_handling.h"
#include <cuda.h>
#include "spdlog/spdlog.h"

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
    if (status != ER_SUCCESS) {
        spdlog::error("Regular Error: [{}] | File: {} | Line: {} | Function: {} | Message: {}", status, file, line, func, msg ? msg : "NONE");
        return status;
    }
    return EC_SUCCESS;
}

template<ErrorType Type, typename StatusType>
int errorCheck(StatusType status, const char* file, int line, const char* func, const char* msg) {
    if constexpr (Type == ErrorType::CUDA) {
        return handleCUDAError(status, file, line, func, msg);
    } else if constexpr (Type == ErrorType::DEFAULT) {
        return handleRegularError(status, file, line, func, msg);
    } else {
        return EC_FAILURE;
    }
}

template int errorCheck<ErrorType::CUDA, CUresult>(CUresult status, const char* file, int line, const char* func, const char* msg);
template int errorCheck<ErrorType::DEFAULT, int>(int status, const char* file, int line, const char* func, const char* msg);
