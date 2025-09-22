#include "ipc.h"
#include "error_handling.h"
#include "server/application.h"
#include "spdlog/spdlog.h"
#include <atomic>

static std::atomic<bool> sigStop{false};

extern "C" {
    int serverInitialize(
        const char* address,
        const int port,
        const int threads
    ) {
        int result = server::Application::create(sigStop, address, port, threads);
        ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to create the server application");

        server::Application& app = server::Application::get();
        result = app.init();
        ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to initialize the server application");

        return EC_SUCCESS;
    }

    int serverRun(void) {
        server::Application& app = server::Application::get();
        int result = app.run();
        ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to start the server application");
        return EC_SUCCESS;
    }

    void stopHandleServer(int signo) {
        spdlog::info("Signal {} received, stopping server...", signo);
        sigStop.store(true, std::memory_order_relaxed);
    }

    int serverDeinitialize(void) {
        server::Application& app = server::Application::get();
        return app.deinit();
    }

} // extern "C"