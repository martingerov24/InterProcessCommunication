#include "application.h"
#include "spdlog/spdlog.h"
#include "fmt/format.h"
#include <zmq.hpp>
#include "ipc.pb.h"
#include "error_handling.h"
#include <random>
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
    const char* endpoint,
    const int port,
    const int receiveTimeoutMs = 3000
) : mCtx(1)
, mSocket(mCtx, zmq::socket_type::dealer)
, mEndpoint(endpoint)
, port(port)
, mReceiveTimeoutMs(receiveTimeoutMs) {}

static std::shared_ptr<client::Application> appPtr = nullptr;

Application& Application::get() {
    assert(appPtr != nullptr);
    return *appPtr;
}

int Application::create(
    const char* address,
    const int port
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
    appPtr = std::shared_ptr<Application>(new Application(address, port));
    return EC_SUCCESS;
}

int Application::init() {
    const std::string endpoint = fmt::format("{}:{}", mEndpoint, port);
    mSocket.set(zmq::sockopt::routing_id, random_identity());
    mSocket.set(zmq::sockopt::linger, 100);
    mSocket.set(zmq::sockopt::rcvtimeo, mReceiveTimeoutMs);
    mSocket.connect(endpoint);
    return EC_SUCCESS;
}

int Application::deinit() {
    mSocket.close(); // ZMQ handles the context cleanup, and this won't make any issue if called multiple times.
    return EC_SUCCESS;
}

Application::~Application() {
    deinit();
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
    if (result.has_value() == false) {
        spdlog::warn("Failed to send message");
        return EC_FAILURE;
    }
    return EC_SUCCESS;
}

int Application::recvEnvelope(ipc::EnvelopeResp& out) {
    zmq::message_t frame;
    const zmq::recv_result_t result = mSocket.recv(frame, zmq::recv_flags::none);

    if (result.has_value() == false) {
        spdlog::warn("Timeout or receive error");
        return EC_FAILURE;
    }

    if (out.ParseFromArray(frame.data(), static_cast<int>(frame.size()) == false)) {
        spdlog::error("Failed to parse EnvelopeResp");
        return EC_FAILURE;
    }
    return EC_SUCCESS;
}

int Application::submitBlocking(
    const ipc::SubmitRequest& req,
    ipc::SubmitResponse& out
) {
    ipc::EnvelopeReq env;
    ipc::SubmitRequest toSend;
    toSend.set_mode(ipc::BLOCKING);
    *env.mutable_submit() = std::move(req);
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
    ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to send EnvelopeReq");

    ipc::EnvelopeResp resp;
    result = recvEnvelope(resp);
    ERROR_CHECK(ErrorType::DEFAULT, result, "Timeout or receive error (EnvelopeResp)");

    if (resp.has_get() == false) {
        spdlog::error("Protocol error: missing get in EnvelopeResp");
        return EC_FAILURE;
    }
    out = resp.get();
    return EC_SUCCESS;
}
