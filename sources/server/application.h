#pragma once
#include <atomic>
#include "zmq.hpp"
#include <vector>

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
        zmq::context_t mCtx{1};
        zmq::socket_t mRouter{mCtx, zmq::socket_type::router};
        std::vector<std::string> mClients;
        std::atomic<bool> mInitialized{false};
        const char* mAddress;
        int mPort;
    };
} // namespace server
