#pragma once

#ifdef __cplusplus
extern "C" {
#endif
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

    int clientInitialize(
        const char* address,
        const int port
    );

    int clientStart(void);

    int clientDeinitialize(void);
#ifdef __cplusplus
} // extern "C"
#endif