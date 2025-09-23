#include "application.h"
#include <zmq_addon.hpp> // For zmq::recv_multipart
#include "signal.h"
#include "error_handling.h"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include "algorithm_runner.h"
#include "ipc.h"
using namespace server;

static std::shared_ptr<server::Application> appPtr = nullptr;

Application::Application(
    const std::atomic<bool>& sigStop,
    const char* address,
    const int port,
    const int threads
) noexcept
: mAddress(address)
, mPort(port)
, mThreads(threads)
, mSigStop(sigStop)
{}

Application& Application::get() {
    assert(appPtr != nullptr);
    return *appPtr;
}

int Application::create(
    const std::atomic<bool>& sigStop,
    const char* address,
    const int port,
    const int threads
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
    appPtr = std::shared_ptr<Application>(new Application(sigStop, address, port, threads));
    return EC_SUCCESS;
}

int Application::init() {
    if (appPtr == nullptr) {
        spdlog::error("Application instance is not created");
        return EC_FAILURE;
    }
    if (mInitialized == true) {
        spdlog::error("Application is already initialized");
        return EC_FAILURE;
    }
    spdlog::info("Initializing Application at {}:{}", mAddress, mPort);
    int result = mAlgoRunner.init(mThreads);
    RETURN_IF_ERROR(ErrorType::DEFAULT, result, "Failed to initialize AlgoRunner");
    const std::string bindAddress = fmt::format("tcp://{}:{}", mAddress, mPort);
    try {
        mRouter.bind(bindAddress);
    } catch (const zmq::error_t& e) {
        spdlog::error("Failed to bind ROUTER socket at {}: {} (errno={})", bindAddress, e.what(), e.num());
        return EC_FAILURE;
    }

    mInitialized.store(true);
    return EC_SUCCESS;
}

int Application::deinit() {
    if (appPtr == nullptr) {
        spdlog::error("Application instance is not created");
        return EC_SUCCESS;
    }
    if (mInitialized == false) {
        spdlog::error("Application is not initialized");
        return EC_FAILURE;
    }

    int result = mAlgoRunner.deinit();
    RETURN_IF_ERROR(ErrorType::DEFAULT, result, "Failed to deinitialize AlgoRunner");

    mInitialized.store(false);
    mRouter.close();

    spdlog::info("Deinitializing Application");
    return EC_SUCCESS;
}

static bool clientHasCapabilityFor(
    const ipc::SubmitRequest& sreq,
    uint8_t clientCaps
) {
    uint8_t required = 0;

    if (sreq.has_math()) {
        switch (sreq.math().op()) {
        case ipc::MATH_ADD: required = ExecFunFlags::ADD; break;
        case ipc::MATH_SUB: required = ExecFunFlags::SUB; break;
        case ipc::MATH_MUL: required = ExecFunFlags::MULT; break;
        case ipc::MATH_DIV: required = ExecFunFlags::DIV;  break;
        default: return false;
        }
    } else if (sreq.has_str()) {
        switch (sreq.str().op()) {
        case ipc::STR_CONCAT:     required = ExecFunFlags::CONCAT;     break;
        case ipc::STR_FIND_START: required = ExecFunFlags::FIND_START; break;
        default: return false;
        }
    } else {
        return false;
    }
    return (clientCaps & required) != 0;
}

int Application::handleEnvelope(
    const ipc::EnvelopeReq& request,
    const uint8_t clientExecCaps,
    ipc::EnvelopeResp& response
) const {
    switch (request.req_case()) {
    case ipc::EnvelopeReq::kSubmit: {
        const ipc::SubmitRequest& sreq = request.submit();
        bool capValid = clientHasCapabilityFor(sreq, clientExecCaps);
        if (capValid == false) {
            response.mutable_submit()->set_status(ipc::ST_ERROR_INVALID_INPUT);
            return EC_SUCCESS;
        }
        ipc::SubmitResponse sresp;
        int result = mAlgoRunner.run(sreq, sresp);
        *response.mutable_submit() = std::move(sresp);
        return result;
    }
    case ipc::EnvelopeReq::kGet: {
        const ipc::GetRequest& greq = request.get();
        ipc::GetResponse gresp;
        int result = mAlgoRunner.get(greq, gresp);
        *response.mutable_get() = std::move(gresp);
        return result;
    }
    case ipc::EnvelopeReq::REQ_NOT_SET:
    default:
        response.mutable_get()->set_status(ipc::ST_ERROR_INVALID_INPUT);
        return EC_FAILURE;
    }
}

int Application::run() {
    if (mInitialized == false) {
        spdlog::error("Application is not initialized");
        return EC_FAILURE;
    }
    spdlog::info("Server running at {}", mAddress);
    int result = EC_SUCCESS;
    while (
        mInitialized.load(std::memory_order_relaxed) &&
        mSigStop.load(std::memory_order_relaxed) == false
    ) {
        try {
            std::vector<zmq::message_t> recvMsgs;
            zmq::recv_result_t zmqResult = zmq::recv_multipart(mRouter, std::back_inserter(recvMsgs));
            if (zmqResult.has_value() == false) {
                continue;
            }
            std::string clientId = recvMsgs[0].to_string();
            std::string payload = recvMsgs.back().to_string();
            auto sendBadResponse = [&] () {
                ipc::EnvelopeResp err;
                err.mutable_get()->set_status(ipc::ST_ERROR_INVALID_INPUT);
                std::string buf; err.SerializeToString(&buf);
                zmq::message_t reply(buf.size());
                memcpy(reply.data(), buf.data(), buf.size());
                zmq::send_result_t resultSend = mRouter.send(recvMsgs[0], zmq::send_flags::sndmore);
                PRINT_ERROR_NO_RET(ErrorType::ZMQ_SEND, resultSend, "Failed to send error response to client");
                resultSend = mRouter.send(reply, zmq::send_flags::none);
                PRINT_ERROR_NO_RET(ErrorType::ZMQ_SEND, resultSend, "Failed to send error response to client");
            };
            if (mClientExecCaps.find(clientId) == mClientExecCaps.end()) {
                spdlog::info("New client connected: {}", clientId);
                ipc::FirstHandshake handshake;
                if (handshake.ParseFromString(payload) == false) {
                    spdlog::error("Bad FirstHandshake from client {}", clientId);
                    sendBadResponse();
                    continue;
                }
                // The cast can happen "automatically", but I want to show that we are casting from uint32 to uint8
                uint8_t funcFlags = static_cast<uint8_t>(handshake.exec_functions());
                bool capsOk = verifyExecCaps(funcFlags);
                if (capsOk == false) {
                    sendBadResponse();
                }
                mClientExecCaps[clientId] = funcFlags;
                continue;
            }
            ipc::EnvelopeReq request;
            if (request.ParseFromString(payload) == false) {
                spdlog::error("Bad EnvelopeReq from client {}", clientId);
                sendBadResponse();
                continue;
            }
            ipc::EnvelopeResp envelopeResp;
            result = handleEnvelope(request, mClientExecCaps[clientId], envelopeResp);
            PRINT_ERROR_NO_RET(ErrorType::DEFAULT, result, "Failed to handle EnvelopeReq");
            std::string serializedResponse;
            if (envelopeResp.SerializeToString(&serializedResponse) == false) {
                spdlog::error("Failed to serialize response for client {}", clientId);
                continue;
            }
            zmq::message_t idFrame(clientId.data(), clientId.size());
            zmq::message_t body(serializedResponse.data(), serializedResponse.size());

            zmq::send_result_t bytesSend = mRouter.send(idFrame, zmq::send_flags::sndmore);
            RETURN_IF_ERROR(ErrorType::ZMQ_SEND, bytesSend, "Failed to send response to client");

            bytesSend = mRouter.send(body, zmq::send_flags::none);
            RETURN_IF_ERROR(ErrorType::ZMQ_SEND, bytesSend, "Failed to send response to client");
        } catch (const zmq::error_t& e) {
            if (e.num() == EINTR || e.num() == ETERM) {
                spdlog::info("ROUTER interrupted (errno={}), shutting down", e.num());
                break;
            } else {
                spdlog::error("ZeroMQ error: {}", e.what());
                result = EC_FAILURE;
                break;
            }
        } catch (const std::exception& e) {
            spdlog::error("Standard exception: {}", e.what());
            result = EC_FAILURE;
            break;
        } catch (...) {
            spdlog::error("Unknown exception occurred");
            result = EC_FAILURE;
            break;
        }
    }
    return EC_SUCCESS;
}

Application::~Application() {
    if (mInitialized == true) {
        spdlog::warn("Application is being deinitialized in destructor");
        int result = deinit();
        PRINT_ERROR_NO_RET(ErrorType::DEFAULT, result, "Failed to deinitialize Application in destructor");
    }
}
