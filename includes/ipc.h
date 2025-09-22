#pragma once
#include "stdint.h"
#include "stdbool.h"

// Ensures C-style linkage for C++ compilers, allowing these functions to be called from both C and C++.
#ifdef __cplusplus
extern "C" {
#endif

    // Enum defining a bitmask for various function capabilities.
    // This allows a client to specify which operations it can ask the server to execute.
    // Each value is a power of 2, so they can be combined using bitwise OR.
    enum ExecFunFlags : uint8_t {
        ADD        = 1 << 0, // Flag for addition capability
        SUB        = 1 << 1, // Flag for subtraction capability
        MULT       = 1 << 2, // Flag for multiplication capability
        DIV        = 1 << 3, // Flag for division capability
        CONCAT     = 1 << 4, // Flag for string concatenation capability
        FIND_START = 1 << 5 // Flag for finding the start of a substring
    };

    // @brief Verifies if the given bitmask of execution capabilities is valid.
    // @param execFunFlags A bitmask of ExecFunFlags.
    // @return true if the flags are valid, false otherwise.
    bool verifyExecCaps(const uint8_t execFunFlags);

    // --------------------------- SERVER API ---------------------------

    /// @brief Initializes the server at the specified address and port.
    /// @param address Starts the server at 0.0.0.0 or localhost.
    /// @param port The port number to bind the server to.
    /// @param threads The number of threads the server will use to handle requests.
    /// @return An error code; 0 for success, non-zero for failure.
    int serverInitialize(
        const char* address,
        const int port,
        const int threads
    );

    /// @brief Runs the server in a blocking mode, listening for client connections.
    /// @return An error code; 0 for success, non-zero for failure.
    int serverRun(void);

    /// @brief Stops the server and deallocates all its resources.
    /// @return An error code; 0 for success, non-zero for failure.
    int serverDeinitialize(void);

    /// @brief A non-blocking signal handler to gracefully stop the server.
    /// @param signo The signal number (e.g., SIGINT, SIGTERM).
    void stopHandleServer(int signo);

    // --------------------------- CLIENT API ---------------------------

    /// @brief A non-blocking signal handler to gracefully stop the client.
    /// @param signo The signal number (e.g., SIGINT, SIGTERM).
    void stopHandleClient(int signo);

    /// @brief Registers the functions the client can call on the server.
    /// @return An error code; 0 for success, non-zero for failure.
    int clientRegisterFunctions(void);

    /// @brief Initializes the client to connect to a specific server.
    /// @param address The address of the server, because we are working inside docker, the address must be the container_name.
    /// @param port The port of the server.
    /// @param receiveTimeoutMs The timeout in milliseconds for receiving data from the server.
    /// @param execFunFlags The bitmask of functions the client is capable of executing.
    /// @return An error code; 0 for success, non-zero for failure.
    int clientInitialize(
        const char* address,
        const int port,
        const int receiveTimeoutMs,
        const uint8_t execFunFlags
    );

    /// @brief Starts the client's connection and communication loop.
    /// @return An error code; 0 for success, non-zero for failure.
    int clientStart(void);

    /// @brief Deinitializes the client and cleans up its resources.
    /// @return An error code; 0 for success, non-zero for failure.
    int clientDeinitialize(void);

#ifdef __cplusplus
} // extern "C"
#endif