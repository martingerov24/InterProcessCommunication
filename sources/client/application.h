#pragma once
#include <atomic>
#include "zmq.hpp"
#include "ipc.pb.h"
#include <vector>

namespace client {
    // ZMQ does not allow const socket for send/recv, that's why each of the functions are not const
    struct Application {
    private:
        int sendFirstHandshake();

        int sendEnvelope(const ipc::EnvelopeReq& env);

        int recvEnvelope(ipc::EnvelopeResp& out);

        explicit Application(
            const std::atomic<bool>& sigStop,
            const char* endpoint,
            const int port,
            const int receiveTimeoutMs,
            const uint8_t execFunFlags
        );

    public:
        static Application& get();

        static int create(
            const std::atomic<bool>& sigStop,
            const char* address,
            const int port,
            const int receiveTimeoutMs,
            const uint8_t execFunFlags
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
        );
    private:
        zmq::context_t mCtx;
        zmq::socket_t mSocket;
        const std::string mIdentity;
        const char* mEndpoint;
        const int mReceiveTimeoutMs;
        const int mPort;
        const uint8_t mExecFunFlags;
        const std::atomic<bool>& mSigStop;
    };
} // namespace client
