#include "algorithm_runner.h"
#include "error_handling.h"
#include <functional>
#include <spdlog/spdlog.h>

using namespace server;

namespace server {
    struct AlgoRunnerIpml {
    private:
        ipc::Status runMath(
            const ipc::MathArgs& request,
            ipc::Result& response
        );
        ipc::Status runStr(
            const ipc::StrArgs& request,
            ipc::Result& response
        );
    public:
        int init();

        int deinit();

        int run(
            const ipc::SubmitRequest& request,
            ipc::SubmitResponse& response
        );

        int get(
            const ipc::GetRequest& request,
            ipc::GetResponse& response
        );
    private:
    };
};

// PUBLIC CLASS METHODS
int AlgoRunner::init() {
    if (outImpl != nullptr) {
        spdlog::error("AlgoRunner is already initialized");
        return EC_SUCCESS;
    }
    outImpl = new std::unique_ptr<AlgoRunnerIpml>(new AlgoRunnerIpml());
    return EC_SUCCESS;
}

int AlgoRunner::deinit() {
    if (outImpl == nullptr) {
        spdlog::error("AlgoRunner is not initialized");
        return EC_SUCCESS;
    }
    delete outImpl;
    outImpl = nullptr;
    return EC_SUCCESS;
}

int AlgoRunner::run(
    const ipc::SubmitRequest& request,
    ipc::SubmitResponse& response
) {
    if (outImpl == nullptr) {
        spdlog::error("AlgoRunner is not initialized");
        return EC_FAILURE;
    }
    return (*outImpl)->run(request, response);
}

int AlgoRunner::get(
    const ipc::GetRequest& request,
    ipc::GetResponse& response
) {
    if (outImpl == nullptr) {
        spdlog::error("AlgoRunner is not initialized");
        return EC_FAILURE;
    }
    return (*outImpl)->get(request, response);
}
// ~ PUBLIC CLASS METHODS

// PRIVATE CLASS METHODS
ipc::Status AlgoRunnerIpml::runMath(
    const ipc::MathArgs& request,
    ipc::Result& response
) {
    switch (request.op()) {
    case ipc::MATH_ADD:
        response.set_int_result(request.a() + request.b());
        break;
    case ipc::MATH_SUB:
        response.set_int_result(request.a() - request.b());
        break;
    case ipc::MATH_MUL:
        response.set_int_result(request.a() * request.b());
        break;
    case ipc::MATH_DIV:
        if (request.b() == 0) {
            return ipc::ST_ERROR_DIV_BY_ZERO;
        }
        response.set_int_result(request.a() / request.b());
        break;
    default:
        return ipc::ST_ERROR_INVALID_INPUT;
    }
    return ipc::ST_SUCCESS;
}

ipc::Status AlgoRunnerIpml::runStr(
    const ipc::StrArgs& request,
    ipc::Result& response
) {
    if (request.op() == ipc::STR_CONCAT) {
        std::string r = request.s1() + request.s2();
        if (r.size() > 32) {
            return ipc::Status::ST_ERROR_STRING_TOO_LONG;
        }
        response.set_str_result(r);
    } else if (request.op() == ipc::STR_FIND_START) {
        std::size_t pos = request.s1().find(request.s2());
        if (pos == std::string::npos) {
            return ipc::ST_ERROR_SUBSTR_NOT_FOUND;
        }
        response.set_position(static_cast<int32_t>(pos));
    } else {
        return ipc::ST_ERROR_INVALID_INPUT;
    }
    return ipc::Status::ST_SUCCESS;
}

int AlgoRunnerIpml::run(
    const ipc::SubmitRequest& request,
    ipc::SubmitResponse& response
) {
    const ipc::SubmitMode mode = request.mode();
    if (mode != ipc::SubmitMode::BLOCKING) {
        response.set_status(ipc::ST_ERROR_INVALID_INPUT);
        return EC_SUCCESS;
    }
    ipc::Status result = ipc::ST_SUCCESS;
    ipc::Result* out = response.mutable_result();
    if (request.has_math()) {
        result = runMath(request.math(), *out);
        response.set_status(result);
        ERROR_CHECK_NO_RET(ErrorType::IPC, result, "Failed to run math operation");
    } else if (request.has_str()) {
        result = runStr(request.str(), *out);
        response.set_status(result);
        ERROR_CHECK_NO_RET(ErrorType::IPC, result, "Failed to run string operation");
    } else {
        response.set_status(ipc::ST_ERROR_INVALID_INPUT);
    }
    return EC_SUCCESS;
}

int AlgoRunnerIpml::get(
    const ipc::GetRequest& request,
    ipc::GetResponse& response
) {
    return EC_SUCCESS;
}
// ~ PRIVATE CLASS METHODS