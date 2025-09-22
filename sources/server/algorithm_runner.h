#pragma once
#include "ipc.pb.h"
#include <memory>

namespace server {
    struct AlgoRunnerIpml;
    struct AlgoRunner {
        int init(const int threads);

        int deinit();

        int run(
            const ipc::SubmitRequest& request,
            ipc::SubmitResponse& response
        ) const;

        int get(
            const ipc::GetRequest& request,
            ipc::GetResponse& response
        ) const ;
    private:
        std::unique_ptr<AlgoRunnerIpml>* outImpl = nullptr;
    };
};