#include "algorithm_runner.h"
#include "error_handling.h"
#include <functional>

using namespace server;

int AlgoRunner::run(
    const ipc::SubmitRequest& request,
    ipc::SubmitResponse& response
) {
    const ipc::SubmitMode mode = request.mode();
    if (mode != ipc::SubmitMode::BLOCKING) {
        response.set_status(ipc::ST_ERROR_INVALID_INPUT);
        return EC_SUCCESS;
    }

    ipc::Result* out = response.mutable_result();
    if (request.has_math()) {
        const auto& m = request.math();
        switch (m.op()) {
        case ipc::MATH_ADD:
            out->set_int_result(m.a() + m.b());
            response.set_status(ipc::ST_SUCCESS);
            break;
        case ipc::MATH_SUB:
            out->set_int_result(m.a() - m.b());
            response.set_status(ipc::ST_SUCCESS);
            break;
        case ipc::MATH_MUL:
            out->set_int_result(m.a() * m.b());
            response.set_status(ipc::ST_SUCCESS);
            break;
        case ipc::MATH_DIV:
            if (m.b() == 0) {
                response.set_status(ipc::ST_ERROR_DIV_BY_ZERO);
                return EC_SUCCESS;
            }
            out->set_int_result(m.a() / m.b());
            response.set_status(ipc::ST_SUCCESS);
            break;
        default:
            response.set_status(ipc::ST_ERROR_INVALID_INPUT);
            break;
        }
    } else if (request.has_str()) {
        const ipc::StrArgs& stringArg = request.str();
        if (stringArg.op() == ipc::STR_CONCAT) {
            std::string r = stringArg.s1() + stringArg.s2();
            if (r.size() > 32) {
                response.set_status(ipc::ST_ERROR_STRING_TOO_LONG);
                return EC_SUCCESS;
            }
            out->set_str_result(r);
            response.set_status(ipc::ST_SUCCESS);
        } else if (stringArg.op() == ipc::STR_FIND_START) {
            std::size_t pos = stringArg.s1().find(stringArg.s2());
            if (pos == std::string::npos) {
                response.set_status(ipc::ST_ERROR_SUBSTR_NOT_FOUND);
                return EC_SUCCESS;
            }
            out->set_position(static_cast<int32_t>(pos));
            return EC_SUCCESS;
        } else {
            return ipc::ST_ERROR_INVALID_INPUT;
        }
    } else {
        response.set_status(ipc::ST_ERROR_INVALID_INPUT);
    }
    return EC_SUCCESS;
}

int AlgoRunner::get(
    const ipc::GetRequest& request,
    ipc::GetResponse& response
) {
    return EC_SUCCESS;
}