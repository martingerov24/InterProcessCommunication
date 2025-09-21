#pragma once
#include "ipc.pb.h"
#include <memory>

namespace server {
    struct AlgoRunnerIpml;
    struct AlgoRunner {
        int init(const int threads);

        int deinit();

        int run(
            const ipc::EnvelopeReq& request,
            ipc::EnvelopeResp& response
        );

        int get(
            const ipc::EnvelopeReq& request,
            ipc::EnvelopeResp& response
        );
    private:
        std::unique_ptr<AlgoRunnerIpml>* outImpl = nullptr;
    };
};