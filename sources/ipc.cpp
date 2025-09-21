#include "ipc.h"
#include "error_handling.h"
#include "server/application.h"
#include "client/application.h"
#include "spdlog/spdlog.h"
#include <atomic>

static std::atomic<bool> sigStop{false};

extern "C" {
    int serverInitialize(const char* address, int port) {
        int result = server::Application::create(sigStop, address, port);
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

    void stopHandle(int signo) {
        sigStop.store(true);
        spdlog::info("Stopping Application");
    }

    int serverDeinitialize(void) {
        server::Application& app = server::Application::get();
        return app.deinit();
    }

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

    int clientDeinitialize(void) {
        client::Application& app = client::Application::get();
        return app.deinit();
    }

} // extern "C"