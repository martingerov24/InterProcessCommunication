#include "application.h"
#include <zmq_addon.hpp> // For zmq::recv_multipart
#include "signal.h"
#include "error_handling.h"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include "algorithm_runner.h"

using namespace server;

static std::shared_ptr<server::Application> appPtr = nullptr;

Application::Application(
    const std::atomic<bool>& sigStop,
    const char* address,
    const int port
) noexcept
: mAddress(address)
, mPort(port)
, sigStop(sigStop) {}

Application& Application::get() {
    assert(appPtr != nullptr);
    return *appPtr;
}

int Application::create(
    const std::atomic<bool>& sigStop,
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
    appPtr = std::shared_ptr<Application>(new Application(sigStop, address, port));
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
    int result = mAlgoRunner.init();
    ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to initialize AlgoRunner");
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
    ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to deinitialize AlgoRunner");

    mInitialized.store(false);
    mRouter.close();

    spdlog::info("Deinitializing Application");
    return EC_SUCCESS;
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
        sigStop.load(std::memory_order_relaxed) == false
    ) {
        try {
            std::vector<zmq::message_t> recvMsgs;
            zmq::recv_result_t zmqResult = zmq::recv_multipart(mRouter, std::back_inserter(recvMsgs));
            if (zmqResult.has_value() == false) {
                continue;
            }
            std::string clientId = recvMsgs[0].to_string();
            std::string payload = recvMsgs.back().to_string();
            ipc::EnvelopeReq request;
            bool res = request.ParseFromString(payload);
            if (res == false) {
                spdlog::error("Failed to parse request from client {}", clientId);
                continue;
            }
            if (request.has_submit()) {
                ipc::SubmitResponse response;
                result = mAlgoRunner.run(request.submit(), response);
                ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to process envelope from client");

                ipc::EnvelopeResp envelopeResp;
                *envelopeResp.mutable_submit() = std::move(response);

                std::string serializedResponse;
                if (envelopeResp.SerializeToString(&serializedResponse) == false) {
                    spdlog::error("Failed to serialize response for client {}", clientId);
                    continue;
                }
                zmq::message_t idFrame(clientId.data(), clientId.size());
                zmq::message_t body(serializedResponse.data(), serializedResponse.size());

                zmq::send_result_t bytesSend = mRouter.send(idFrame, zmq::send_flags::sndmore);
                ERROR_CHECK(ErrorType::ZMQ_SEND, bytesSend, "Failed to send response to client");

                bytesSend = mRouter.send(body, zmq::send_flags::none);
                ERROR_CHECK(ErrorType::ZMQ_SEND, bytesSend, "Failed to send response to client");
            }
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
        ERROR_CHECK_NO_RET(ErrorType::DEFAULT, result, "Failed to deinitialize Application in destructor");
    }
}
