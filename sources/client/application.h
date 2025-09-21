#pragma once
#include <atomic>
#include "zmq.hpp"
#include "ipc.pb.h"
#include <vector>

namespace client {
    struct Application {
    private:
        int sendEnvelope(const ipc::EnvelopeReq& env);

        int recvEnvelope(ipc::EnvelopeResp& out);

        explicit Application(
            const std::atomic<bool>& sigStop,
            const char* endpoint,
            const int port,
            const int receiveTimeoutMs = 3000
        );

    public:
        static Application& get();

        static int create(
            const std::atomic<bool>& sigStop,
            const char* address,
            int port
        ) noexcept;

        ~Application();

        int init();

        int run();

        int deinit();

        int submitBlocking(
            const ipc::SubmitRequest& req,
            ipc::SubmitResponse& out
        );

        int submitNonBlocking(
            const ipc::SubmitRequest& req,
            ipc::SubmitResponse& out
        );

        int getResult(
            const ipc::Ticket& ticket,
            const ipc::GetWaitMode waitMode,
            const uint32_t timeoutMs,
            ipc::GetResponse& out
        ) ;
    private:
        zmq::context_t mCtx;
        zmq::socket_t mSocket;
        const char* mEndpoint;
        const int mReceiveTimeoutMs;
        const int port;
        const std::atomic<bool>& sigStop;
    };
} // namespace client
