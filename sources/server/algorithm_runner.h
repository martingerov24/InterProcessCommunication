#pragma once
#include "ipc.pb.h"

namespace server {
    struct AlgoRunner {
        int run(
            const ipc::SubmitRequest& request,
            ipc::SubmitResponse& response
        );
        int get(
            const ipc::GetRequest& request,
            ipc::GetResponse& response
        );
    };
};