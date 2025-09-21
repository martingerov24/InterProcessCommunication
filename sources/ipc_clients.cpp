#include "ipc.h"
#include "error_handling.h"
#include "client/application.h"
#include "spdlog/spdlog.h"
#include <atomic>

static std::atomic<bool> sigStop{false};
extern "C" {
    int clientInitialize(const char* address, int port) {
        int result = client::Application::create(sigStop, address, port);
        ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to create the client application");

        client::Application& app = client::Application::get();
        result = app.init();
        ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to initialize the client application");

        return EC_SUCCESS;
    }

    int clientStart(void) {
        client::Application& app = client::Application::get();
        int result = app.run();
        ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to start the client application");
        return EC_SUCCESS;
    }

    void stopHandleClient(int signo) {
        spdlog::info("Signal {} received, stopping client...", signo);
        sigStop.store(true, std::memory_order_relaxed);
    }

    int clientDeinitialize(void) {
        client::Application& app = client::Application::get();
        return app.deinit();
    }

} // extern "C"