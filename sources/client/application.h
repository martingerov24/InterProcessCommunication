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
            const char* endpoint,
            const int port,
            const int receiveTimeoutMs
        );

    public:
        static Application& get();

        static int create(const char* address, int port) noexcept;

        ~Application();

        int init();

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

        static ipc::SubmitRequest makeMath(
            ipc::MathOp op,
            int32_t a,
            int32_t b
        );

        static ipc::SubmitRequest makeStr(
            ipc::StrOp op,
            const std::string& s1,
            const std::string& s2
        );
    private:
        zmq::context_t mCtx;
        zmq::socket_t mSocket;
        const char* mEndpoint;
        const int mReceiveTimeoutMs;
        const int port;
    };
} // namespace client
