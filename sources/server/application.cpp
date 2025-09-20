#include "application.h"
#include <zmq.hpp>
#include "signal.h"
#include "error_handling.h"
#include <spdlog/spdlog.h>

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

    spdlog::info("Deinitializing Application");
    appPtr.reset();
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

    spdlog::info("Server running at {}", mAddress);
    while (mInitialized.load(std::memory_order_relaxed) && sigStop.load(std::memory_order_relaxed) == false) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return EC_SUCCESS;
}

Application::~Application() {
    if (mInitialized == true) {
        spdlog::warn("Application is being deinitialized in destructor");
        int result = deinit();
        ERROR_CHECK_NO_RET(ErrorType::DEFAULT, result, "Failed to deinitialize Application in destructor");
    }
    if (appPtr == nullptr) {
        return;
    }
    Application& app = Application::get();
    int result = app.deinit();
    ERROR_CHECK_NO_RET(ErrorType::DEFAULT, result, "Failed to deinitialize Application");
}
