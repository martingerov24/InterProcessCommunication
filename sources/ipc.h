#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Initialize the server at the specified address and port.
/// @param address For IPC will be 127 or localhost. For TCP, it can be any valid IP address or hostname.
/// @param port Port number to bind the server to.
/// @return error code, 0 for success, any other value indicates failure.
int server_initialize(const char* address, int port);

/// @brief Run the server (blocking call).
/// @return error code, 0 for success, any other value indicates failure.
int server_run(void);

/// @brief Stop the server and clean up resources.
/// @return error code, 0 for success, any other value indicates failure.
int server_deinitialize(void);

/// @brief Stop the server (non-blocking call).
void server_stop(int signo);

int client_initialize(const char* address, int port);

int client_deinitialize(void);

#ifdef __cplusplus
} // extern "C"
#endif