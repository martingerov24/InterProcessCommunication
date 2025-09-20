#pragma once
#include <memory>
#include <string>

namespace server {
    /// Singleton class representing the server application.
    struct Application {
        Application() = default;
        int init(const char* address, int port) noexcept;
        // Start the server loop (blocking);
        int run();
        int deinit();
        ~Application();
    };
} // namespace server