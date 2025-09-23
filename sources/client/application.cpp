#include "application.h"
#include "spdlog/spdlog.h"
#include "fmt/format.h"
#include <zmq.hpp>
#include "ipc.pb.h"
#include "error_handling.h"
#include <random>
#include <zmq_addon.hpp> // For zmq::recv_multipart
#include <cctype>
#include <unordered_map>

using namespace client;

static std::string random_identity(std::size_t n = 8) {
    static const char chars[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> dist(0, sizeof(chars) - 2);
    std::string id; id.reserve(n);
    for (std::size_t i = 0; i < n; ++i) id.push_back(chars[dist(rng)]);
    return id;
}

Application::Application(
    const std::atomic<bool>& sigStop,
    const char* address,
    const int port,
    const int receiveTimeoutMs,
    const uint8_t execFunFlags
) : mCtx(1)
, mSocket(mCtx, zmq::socket_type::dealer)
, mIdentity(random_identity())
, mEndpoint(address)
, mReceiveTimeoutMs(receiveTimeoutMs)
, mPort(port)
, mExecFunFlags(execFunFlags)
, mSigStop(sigStop) {}

static std::shared_ptr<client::Application> appPtr = nullptr;

Application& Application::get() {
    assert(appPtr != nullptr);
    return *appPtr;
}

int Application::create(
    const std::atomic<bool>& sigStop,
    const char* address,
    const int port,
    const int receiveTimeoutMs,
    const uint8_t execFunFlags
) noexcept {
    static int instanceCount = 0;
    if (instanceCount >= 1) {
        spdlog::error("Only one instance of Application is allowed");
        return EC_FAILURE;
    }
    instanceCount++;
    if (appPtr != nullptr) {
        spdlog::error("Application instance is already created");
        return EC_FAILURE;
    }
    appPtr = std::shared_ptr<Application>(
        new Application(
            sigStop,
            address,
            port,
            receiveTimeoutMs,
            execFunFlags
        )
    );
    return EC_SUCCESS;
}

int Application::init() {
    const std::string endpoint = fmt::format("tcp://{}:{}", mEndpoint, mPort);
    try {
        mSocket.set(zmq::sockopt::routing_id, mIdentity);
        mSocket.set(zmq::sockopt::linger, 100);
        mSocket.set(zmq::sockopt::rcvtimeo, mReceiveTimeoutMs);
        mSocket.connect(endpoint);
        int result = sendFirstHandshake();
        RETURN_IF_ERROR(ErrorType::DEFAULT, result, "Failed to send first handshake");
    } catch (const zmq::error_t& e) {
        spdlog::error("Failed to connect to {}: {}", endpoint, e.what());
        return EC_FAILURE;
    }
    return EC_SUCCESS;
}

int Application::deinit() {
    mSocket.close(); // ZMQ handles the context cleanup, safe if called multiple times.
    return EC_SUCCESS;
}

Application::~Application() {
    deinit();
}

int Application::sendFirstHandshake() {
    ipc::FirstHandshake handshake;
    handshake.set_client_name(mIdentity);
    uint32_t funcFlags = static_cast<uint32_t>(mExecFunFlags);
    handshake.set_exec_functions(funcFlags);
    std::string buf;
    if (handshake.SerializeToString(&buf) == false) {
        spdlog::error("Failed to serialize FirstHandshake");
        return EC_FAILURE;
    }
    zmq::message_t frame(buf.size());
    memcpy(frame.data(), buf.data(), buf.size());
    zmq::send_result_t result = mSocket.send(frame, zmq::send_flags::none);
    RETURN_IF_ERROR(ErrorType::ZMQ_SEND, result, "Failed to send message");
    return EC_SUCCESS;
}

int Application::sendEnvelope(const ipc::EnvelopeReq& env) {
    std::string buf;
    if (env.SerializeToString(&buf) == false) {
        spdlog::error("Failed to serialize EnvelopeReq");
        return EC_FAILURE;
    }
    zmq::message_t frame(buf.size());
    memcpy(frame.data(), buf.data(), buf.size());
    zmq::send_result_t result = mSocket.send(frame, zmq::send_flags::none);
    RETURN_IF_ERROR(ErrorType::ZMQ_SEND, result, "Failed to send message");
    return EC_SUCCESS;
}

int Application::recvEnvelope(ipc::EnvelopeResp& out) {
    std::vector<zmq::message_t> frames;
    zmq::recv_result_t ok = zmq::recv_multipart(mSocket, std::back_inserter(frames));
    if (ok.has_value() == false || frames.empty()) {
        spdlog::warn("Timeout or receive error");
        return EC_FAILURE;
    }

    const zmq::message_t& frame = frames.back();
    if (!out.ParseFromArray(frame.data(), static_cast<int>(frame.size()))) {
        spdlog::error("Failed to parse EnvelopeResp (sz={})", (int)frame.size());
        return EC_FAILURE;
    }
    return EC_SUCCESS;
}

int Application::submitBlocking(
    const ipc::SubmitRequest& req,
    ipc::SubmitResponse& out
) {
    ipc::EnvelopeReq env;
    ipc::SubmitRequest toSend = req;
    toSend.set_mode(ipc::BLOCKING);
    *env.mutable_submit() = std::move(toSend);
    int result = sendEnvelope(env);
    if (result != EC_SUCCESS) {
        out.set_status(ipc::ST_ERROR_INTERNAL);
        spdlog::error("Failed to send EnvelopeReq");
        return EC_FAILURE;
    }

    ipc::EnvelopeResp resp;
    result = recvEnvelope(resp);
    if (result != EC_SUCCESS) {
        out.set_status(ipc::ST_ERROR_INTERNAL);
        spdlog::error("Timeout or receive error (EnvelopeResp)");
        return EC_FAILURE;
    }

    if (resp.has_submit() == false) {
        out.set_status(ipc::ST_ERROR_INTERNAL);
        spdlog::error("Protocol error: missing submit in EnvelopeResp");
        return EC_FAILURE;
    }
    out = std::move(resp.submit());
    return EC_SUCCESS;
}

int Application::submitNonBlocking(
    const ipc::SubmitRequest& req,
    ipc::SubmitResponse& out
) {
    ipc::EnvelopeReq env;
    ipc::SubmitRequest toSend = req;
    toSend.set_mode(ipc::NONBLOCKING);
    *env.mutable_submit() = std::move(toSend);
    sendEnvelope(env);

    ipc::EnvelopeResp resp;
    int result = recvEnvelope(resp);
    if (result != EC_SUCCESS) {
        out.set_status(ipc::ST_ERROR_INTERNAL);
        spdlog::error("Timeout or receive error (EnvelopeResp)");
        return EC_FAILURE;
    }
    if (resp.has_submit() == false) {
        out.set_status(ipc::ST_ERROR_INTERNAL);
        spdlog::error("Protocol error: missing submit in EnvelopeResp");
        return EC_FAILURE;
    }
    out = std::move(resp.submit());
    return EC_SUCCESS;
}

int Application::getResult(
    const ipc::Ticket& ticket,
    const ipc::GetWaitMode waitMode,
    const uint32_t timeoutMs,
    ipc::GetResponse& out
) {
    ipc::EnvelopeReq env;
    ipc::GetRequest& g = *env.mutable_get();
    *g.mutable_ticket() = ticket;
    g.set_wait_mode(waitMode);
    if (waitMode == ipc::WAIT_UP_TO) {
        g.set_timeout_ms(timeoutMs);
    }

    int result = sendEnvelope(env);
    RETURN_IF_ERROR(ErrorType::DEFAULT, result, "Failed to send EnvelopeReq");

    ipc::EnvelopeResp resp;
    result = recvEnvelope(resp);
    RETURN_IF_ERROR(ErrorType::DEFAULT, result, "Timeout or receive error (EnvelopeResp)");

    if (resp.has_get() == false) {
        spdlog::error("Protocol error: missing get in EnvelopeResp");
        return EC_FAILURE;
    }
    out = resp.get();
    return EC_SUCCESS;
}

static bool insensitiveEquals(const char* a, const char* b) {
    for (; *a && *b; ++a, ++b) {
        if (std::tolower((unsigned char)*a) != std::tolower((unsigned char)*b)) {
            return false;
        }
    }
    return *a == *b;
}

static bool isNonblockToken(const char* s) {
    return insensitiveEquals(s, "non-block") ||
        insensitiveEquals(s, "nonblock") ||
        insensitiveEquals(s, "non_block") ||
        insensitiveEquals(s, "async");
}

static bool isBlockToken(const char* s) {
    return insensitiveEquals(s, "block") ||
        insensitiveEquals(s, "blocking") ||
        insensitiveEquals(s, "sync");
}

static void printSubmit(const ipc::SubmitResponse& response) {
    PRINT_ERROR_NO_RET(ErrorType::IPC, response.status(), "Error in response");
    if (response.has_ticket()) {
        printf("ticket=%llu\n", (unsigned long long)response.ticket().req_id());
    }
    if (response.has_result()) {
        const ipc::Result& value = response.result();
        switch (value.value_case()) {
        case ipc::Result::kIntResult:
            printf("Result: Int=%d\n", value.int_result());
            break;
        case ipc::Result::kPosition:
            printf("Result: Pos=%d\n", value.position());
            break;
        case ipc::Result::kStrResult:
            printf("Result: Str=%s\n", value.str_result().c_str());
            break;
        case ipc::Result::VALUE_NOT_SET:
        default:
            printf("No result set\n");
            break;
        }
    }
}

static void printGet(const ipc::GetResponse& response) {
    PRINT_ERROR_NO_RET(ErrorType::IPC, response.status(), "Error in response");
    if (!response.has_result()) {
        if (response.status() == ipc::ST_NOT_FINISHED) {
            printf("Result: NOT FINISHED\n");
        } else {
            printf("No result payload\n");
        }
        return;
    }
    const ipc::Result& value = response.result();
    switch (value.value_case()) {
    case ipc::Result::kIntResult:
        printf("Result: Int=%d\n", value.int_result());
        break;
    case ipc::Result::kPosition:
        printf("Result: Pos=%d\n", value.position());
        break;
    case ipc::Result::kStrResult:
        printf("Result: Str=%s\n", value.str_result().c_str());
        break;
    case ipc::Result::VALUE_NOT_SET:
    default:
        break;
    }
}

static void printHelp() {
    printf(
        "Commands:\n"
        "  block/non-block add a b        \n"
        "  block/non-block sub a b        \n"
        "  block/non-block mult a b       \n"
        "  block/non-block div a b        \n"
        "  block/non-block concat s1 s2   \n"
        "  block/non-block find hay needle\n"
        "  get <ticket> [nowait | wait <ms>]  (retrieve result for non-blocking ticket)\n"
        "  list                               (list pending tickets)\n"
        "  quit | exit\n"
    );
}

namespace client {
    ipc::SubmitRequest makeMath(
        ipc::MathOp op,
        int32_t a,
        int32_t b
    ) {
        ipc::SubmitRequest s;
        auto* m = s.mutable_math();
        m->set_op(op);
        m->set_a(a);
        m->set_b(b);
        return s;
    }

    ipc::SubmitRequest makeStr(
        ipc::StrOp op,
        const std::string& s1,
        const std::string& s2
    ) {
        ipc::SubmitRequest s;
        auto* st = s.mutable_str();
        st->set_op(op);
        st->set_s1(s1);
        st->set_s2(s2);
        return s;
    }
};

int Application::run() {
    client::Application& app = client::Application::get();
    std::unordered_map<uint64_t, ipc::Ticket> pending;

    printf("Client started. Type 'help' for commands.\n");

    char line[512];
    int result = EC_SUCCESS;
    while (mSigStop.load(std::memory_order_relaxed) == false) {
        printf(">> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        size_t len = strlen(line);
        if (len && line[len - 1] == '\n') {
            line[len - 1] = 0;
        }
        if (line[0] == 0) {
            continue;
        }

        if (insensitiveEquals(line, "quit") || insensitiveEquals(line, "exit")) {
            break;
        }
        if (insensitiveEquals(line, "help")) {
            printHelp();
            continue;
        }

        char buf[512]; std::snprintf(buf, sizeof(buf), "%s", line);

        // First token may be "get", "list", or a mode (block/non-block)
        char tok1[32] = {0};
        if (std::sscanf(buf, "%31s", tok1) != 1) {
            printf("Bad Command. Type 'help'\n");
            continue;
        }

        // ----- GET COMMAND -----
        if (insensitiveEquals(tok1, "get")) {
            // Forms:
            //   get <ticket>
            //   get <ticket> nowait
            //   get <ticket> wait <ms>
            char ticketStr[64] = {0};
            char waitTok[32] = {0};
            unsigned ms = 0;
            int n = std::sscanf(buf, "%*31s %63s %31s %u", ticketStr, waitTok, &ms);
            if (n < 1) {
                printf("Usage: get <ticket> [nowait | wait <ms>]\n");
                continue;
            }
            uint64_t ticketId = std::strtoull(ticketStr, nullptr, 10);
            auto it = pending.find(ticketId);
            if (it == pending.end()) {
                printf("Unknown or already consumed ticket: %llu\n",
                       (unsigned long long)ticketId);
                continue;
            }
            ipc::GetWaitMode mode = ipc::NO_WAIT;
            uint32_t timeoutMs = 0;
            if (n >= 2) {
                if (insensitiveEquals(waitTok, "nowait")) {
                    mode = ipc::NO_WAIT;
                } else if (insensitiveEquals(waitTok, "wait")) {
                    mode = ipc::WAIT_UP_TO;
                    timeoutMs = (n >= 3) ? ms : 0u;
                } else {
                    printf("Invalid wait mode. Use: nowait | wait <ms>\n");
                    continue;
                }
            }
            ipc::GetResponse gres;
            int rc = app.getResult(it->second, mode, timeoutMs, gres);
            if (rc != EC_SUCCESS) {
                printf("Error getting result (transport)\n");
                continue;
            }
            printGet(gres);
            // On finished (either success or error, but not NOT_FINISHED), erase ticket
            if (gres.status() != ipc::ST_NOT_FINISHED) {
                pending.erase(it);
            }
            continue;
        }

        // ----- LIST COMMAND -----
        if (insensitiveEquals(tok1, "list")) {
            if (pending.empty()) {
                printf("No pending tickets.\n");
            } else {
                printf("Pending tickets:\n");
                for (const auto& kv : pending) {
                    printf("  %llu\n", (unsigned long long)kv.first);
                }
            }
            continue;
        }

        // Otherwise, parse as mode + op
        char mode[32] = {0};
        char op[32]   = {0};
        if (std::sscanf(buf, "%31s %31s", mode, op) != 2) {
            printf("Bad Command. Type 'help'\n");
            continue;
        }

        const bool isNonBlocking = isNonblockToken(mode);
        const bool isBlocking = isBlockToken(mode);
        if (isNonBlocking == false && isBlocking == false) {
            printf("First token must be 'block' or 'non-block or get'\n");
            continue;
        }

        ipc::SubmitResponse sresp;

        if (insensitiveEquals(op, "add") ||
            insensitiveEquals(op, "sub") ||
            insensitiveEquals(op, "mult") ||
            insensitiveEquals(op, "div")
        ) {
            int a = 0, b = 0;
            if (std::sscanf(buf, "%*31s %*31s %d %d", &a, &b) != 2) {
                printf("Usage: %s %s a b\n", mode, op);
                continue;
            }
            ipc::MathOp m =
                insensitiveEquals(op,"add") ? ipc::MATH_ADD :
                insensitiveEquals(op,"sub") ? ipc::MATH_SUB :
                insensitiveEquals(op,"mult") ? ipc::MATH_MUL : ipc::MATH_DIV;

            ipc::SubmitRequest req = client::makeMath(m, a, b);
            if (isBlocking) {
                result = app.submitBlocking(req, sresp);
                RETURN_IF_ERROR(ErrorType::DEFAULT, result, "Failed to submit blocking request");
            } else {
                result = app.submitNonBlocking(req, sresp);
                RETURN_IF_ERROR(ErrorType::DEFAULT, result, "Failed to submit non-blocking request");
            }
            if (result == EC_SUCCESS) {
                printSubmit(sresp);
                if (isNonBlocking && sresp.has_ticket()) {
                    pending[sresp.ticket().req_id()] = sresp.ticket();
                }
            } else {
                printf("error sending request\n");
                continue;
            }
        } else if (insensitiveEquals(op, "concat")) {
            char s1[64] = {0}, s2[64] = {0};
            if (std::sscanf(buf, "%*31s %*31s %63s %63s", s1, s2) != 2) {
                printf("Usage: %s concat s1 s2\n", mode);
                continue;
            }
            ipc::SubmitRequest req = client::makeStr(ipc::STR_CONCAT, s1, s2);
            if (isBlocking) {
                result = app.submitBlocking(req, sresp);
            } else {
                result = app.submitNonBlocking(req, sresp);
            }
            if (result == EC_SUCCESS) {
                printSubmit(sresp);
                if (isNonBlocking && sresp.has_ticket()) {
                    pending[sresp.ticket().req_id()] = sresp.ticket();
                }
            } else {
                printf("error sending request\n");
            }
        } else if (insensitiveEquals(op, "find")) {
            char hay[64] = {0}, needle[64] = {0};
            if (std::sscanf(buf, "%*31s %*31s %63s %63s", hay, needle) != 2) {
                printf("Usage: %s find hay needle\n", mode);
                continue;
            }
            ipc::SubmitRequest req = client::makeStr(ipc::STR_FIND_START, hay, needle);
            if (isBlocking) {
                result = app.submitBlocking(req, sresp);
            } else {
                result = app.submitNonBlocking(req, sresp);
            }
            if (result == EC_SUCCESS) {
                printSubmit(sresp);
                if (isNonBlocking && sresp.has_ticket()) {
                    pending[sresp.ticket().req_id()] = sresp.ticket();
                }
            } else {
                printf("Error sending request\n");
            }
        } else {
            printf("Unknown op. Type 'help'\n");
        }
    }
    printf("Exiting...\n");
    return EC_SUCCESS;
}
