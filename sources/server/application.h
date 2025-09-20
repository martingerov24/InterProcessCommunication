#pragma once
#include <atomic>

namespace server {
    /// Singleton class representing the server application.
    struct Application {
    private:
        Application(const char* address, int port) noexcept;
        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;
    public:
        static Application& get();
        static int create(const char* address, int port) noexcept;
        int init();
        // Start the server loop (blocking);
        int run();
        int deinit();
        void stop();
        ~Application();
    private:
        std::atomic<bool> mInitialized{false};
        std::atomic<bool> mRunning{false};
        const char* mAddress;
        int mPort;
    };
} // namespace server
