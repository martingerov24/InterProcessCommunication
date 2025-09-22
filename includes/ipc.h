#pragma once
#include "stdint.h"
#include "stdbool.h"
#ifdef __cplusplus
extern "C" {
#endif
    enum ExecFunFlags : uint8_t {
        ADD        = 1 << 0,
        SUB        = 1 << 1,
        MULT       = 1 << 2,
        DIV        = 1 << 3,
        CONCAT     = 1 << 4,
        FIND_START = 1 << 5
    };

    bool verifyExecCaps(const uint8_t execFunFlags);

    /// @brief Initialize the server at the specified address and port.
    /// @param address For IPC will be 127 or localhost. For TCP, it can be any valid IP address or hostname.
    /// @param port Port number to bind the server to.
    /// @return error code, 0 for success, any other value indicates failure.
    int serverInitialize(
        const char* address,
        const int port,
        const int theads
    );

    /// @brief Run the server (blocking call).
    /// @return error code, 0 for success, any other value indicates failure.
    int serverRun(void);

    /// @brief Stop the server and clean up resources.
    /// @return error code, 0 for success, any other value indicates failure.
    int serverDeinitialize(void);

    /// @brief Stop the server (non-blocking call).
    void stopHandleServer(int signo);

    /// @brief Stop the client (non-blocking call).
    void stopHandleClient(int signo);

    int clientRegisterFunctions(void);

    int clientInitialize(
        const char* address,
        const int port,
        const int receiveTimeoutMs,
        const uint8_t execFunFlags
    );

    int clientStart(void);

    int clientDeinitialize(void);
#ifdef __cplusplus
} // extern "C"
#endif