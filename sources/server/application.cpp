#include "application.h"
#include <zmq_addon.hpp> // For zmq::recv_multipart
#include "signal.h"
#include "error_handling.h"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include "algorithm_runner.h"

using namespace server;

static std::shared_ptr<server::Application> appPtr = nullptr;
static std::atomic<bool> sigStop{false};

Application::Application(
    const char* address,
    const int port
) noexcept
: mAddress(address)
, mPort(port) {}

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
    if (appPtr == nullptr) {
        spdlog::error("Application instance is not created");
        return EC_FAILURE;
    }
    if (mInitialized == true) {
        spdlog::error("Application is already initialized");
        return EC_FAILURE;
    }
    spdlog::info("Initializing Application at {}:{}", mAddress, mPort);

    const char* bindAddress = fmt::format("tcp://{}:{}", mAddress, mPort).c_str();

    mRouter.bind(bindAddress);

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

    mInitialized.store(false);
    mRouter.close();

    spdlog::info("Deinitializing Application");
    return EC_SUCCESS;
}

void Application::stop() {
    if (mInitialized == false) {
        spdlog::error("Application is not initialized");
        return;
    }
    sigStop.store(true, std::memory_order_relaxed);
    spdlog::info("Stopping Application");
}

int Application::run() {
    if (mInitialized == false) {
        spdlog::error("Application is not initialized");
        return EC_FAILURE;
    }
    AlgoRunner algoRunner;
    spdlog::info("Server running at {}", mAddress);
    int result = EC_SUCCESS;
    while (
        mInitialized.load(std::memory_order_relaxed) &&
        sigStop.load(std::memory_order_relaxed) == false
    ) {
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
            result = algoRunner.run(request.submit(), response);
            ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to process envelope from client");

            ipc::EnvelopeResp envelopeResp;
            *envelopeResp.mutable_submit() = std::move(response);

            std::string serializedResponse;
            if (envelopeResp.SerializeToString(&serializedResponse) == false) {
                spdlog::error("Failed to serialize response for client {}", clientId);
                continue;
            }
            zmq::message_t idFrame(clientId.data(), clientId.size());
            zmq::message_t empty;
            zmq::message_t body(serializedResponse.data(), serializedResponse.size());

            mRouter.send(idFrame, zmq::send_flags::sndmore);
            mRouter.send(empty, zmq::send_flags::sndmore);
            mRouter.send(body, zmq::send_flags::none);
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
