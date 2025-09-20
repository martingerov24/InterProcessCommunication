#pragma once
#include <cinttypes>

#define EC_SUCCESS (1)
#define EC_FAILURE (-1)

/// Enumeration defining types of errors handled in the system.
/// If more APIs a re added, extend this enum accordingly. For example, we can add OpenCV, OpenGL, etc.
enum class ErrorType : uint8_t {
    CUDA, ///< CUDA error type.
    DEFAULT ///< Default error type for general use.
};

/// Template function to handle error checking based on the error type.
/// @tparam Type The type of error (e.g., ErrorType::CUDA).
/// @tparam StatusType The type of the status code (CUresult, int).
/// @param status The status code to check.
/// @param file The name of the file where the error occurred.
/// @param line The line number where the error occurred.
/// @param func The name of the function where the error occurred.
/// @param msg Rrror message providing additional context.
/// @return An integer representing the error code or success (EC_SUCCESS).
template<ErrorType Type, typename StatusType>
int errorCheck(StatusType status, const char* file, int line, const char* func, const char* msg);

/// The use of decltype ensures that the exact type of the 'status' parameter is deduced at compile time.
/// We are using templates, so the ifs for the ErrorType can be evaluated at compile time.
#define ERROR_CHECK(type, status, errMsg)                                      \
    do {                                                                       \
        int errorMsg = errorCheck<type, decltype(status)>(status, __FILE__, __LINE__, __FUNCTION__, errMsg); \
        if (errorMsg != EC_SUCCESS) {                                     \
            return errorMsg;                                                   \
        }                                                                      \
    } while (0)

#define ERROR_CHECK_NO_RET(type, status, errMsg)                               \
    do {                                                                       \
        int errorMsg = errorCheck<type, decltype(status)>(status, __FILE__, __LINE__, __FUNCTION__, errMsg); \
    } while (0)