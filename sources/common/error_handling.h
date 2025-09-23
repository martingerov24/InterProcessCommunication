#pragma once
#include <cinttypes>

// Standard success and failure codes. Using a macro for EC_FAILURE allows for easy modification of the failure value.
#define EC_SUCCESS (0)
#define EC_FAILURE (-1)

/// @brief Defines categories for different types of errors.
/// This enumeration allows the error-handling system to use specific logic for different
/// APIs (e.g., ZeroMQ, a custom IPC protocol) by dispatching to the correct handler at compile time.
enum class ErrorType : uint8_t {
    ZMQ_SEND, ///< Category for errors related to ZeroMQ message sending operations.
    IPC,      ///< Category for errors originating from the custom IPC protocol.
    DEFAULT   ///< A generic category for standard integer-based error codes.
};

/// @brief A template function that serves as the central point for all error checks.
///
/// This function uses a template to provide a uniform interface for checking
/// different types of status codes. Its implementation relies on `if constexpr`
/// to select the appropriate error-handling function at compile time based on the `ErrorType`.
/// This approach ensures a single, flexible entry point without runtime overhead.
///
/// @tparam Type The specific category of the error, defined by the `ErrorType` enum.
/// @tparam StatusType The data type of the status code being checked (e.g., `int`, `zmq::send_result_t`, `ipc::Status`).
/// @param status The status code returned by a function call.
/// @param file The source file name (`__FILE__`).
/// @param line The line number in the file (`__LINE__`).
/// @param func The name of the function where the check is made (`__FUNCTION__`).
/// @param msg A custom message to provide additional context for the error.
/// @return Returns `EC_SUCCESS` on success or an appropriate error code on failure.
template<ErrorType Type, typename StatusType>
int errorCheck(StatusType status, const char* file, int line, const char* func, const char* msg);

/// @brief A macro to check for errors and return upon failure.
///
/// This macro wraps a function call, passes its status to `errorCheck`,
/// and immediately returns the error code if the status indicates a failure.
/// The `decltype(status)` ensures the macro is type-safe and works with any status type.
#define RETURN_IF_ERROR(type, status, errMsg)                                                                   \
    do {                                                                                                    \
        int errorMsg = errorCheck<type, decltype(status)>(status, __FILE__, __LINE__, __FUNCTION__, errMsg);\
        if (errorMsg != EC_SUCCESS) {                                                                       \
            return errorMsg;                                                                                \
        }                                                                                                   \
    } while (0)

/// @brief A macro to check for errors without returning.
///
/// This macro is similar to `RETURN_IF_ERROR` but is designed for functions that return `void`.
/// It logs the error but does not interrupt the program's flow. The `(void)errorMsg`
/// cast is used to suppress "unused variable" warnings.
#define PRINT_ERROR_NO_RET(type, status, errMsg)                                                            \
    do {                                                                                                    \
        int errorMsg = errorCheck<type, decltype(status)>(status, __FILE__, __LINE__, __FUNCTION__, errMsg);\
        (void)errorMsg;                                                                                     \
    } while (0)
