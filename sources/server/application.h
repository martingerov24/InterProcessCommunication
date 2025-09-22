#pragma once
#include <atomic>
#include "zmq.hpp"
#include <vector>
#include "algorithm_runner.h"

namespace server {
    /// Singleton class representing the server application.
    struct Application {
    private:
        int handleEnvelope(
            const ipc::EnvelopeReq& request,
            const uint8_t clientExecCaps,
            ipc::EnvelopeResp& response
        ) const;
        explicit Application(
            const std::atomic<bool>& sigStop,
            const char* address,
            const int port,
            const int theads
        ) noexcept;
        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;
    public:
        static Application& get();
        static int create(
            const std::atomic<bool>& sigStop,
            const char* address,
            const int port,
            const int theads
        ) noexcept;
        int init();
        // Start the server loop (blocking);
        int run();
        int deinit();
        ~Application();
    private:
        zmq::context_t mCtx{1};
        zmq::socket_t mRouter{mCtx, zmq::socket_type::router};
        std::unordered_map<std::string, uint8_t> mClientExecCaps;
        AlgoRunner mAlgoRunner;
        const char* mAddress;
        const int mPort;
        const int mThreads;
        const std::atomic<bool>& mSigStop;
        std::atomic<bool> mInitialized{false};
    };
} // namespace server
