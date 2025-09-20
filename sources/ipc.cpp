#include "ipc.h"
#include "application.h"
#include "error_handling.h"

extern "C" {

int initializeServer(const char* address, int port) {
    int result = server::Application::create(address, port);
    ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to create the server application");

    server::Application& app = server::Application::get();
    result = app.init();
    ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to initialize the server application");

    return EC_SUCCESS;
}

int runServer(void) {
    server::Application& app = server::Application::get();
    int result = app.run();
    ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to start the server application");
    return EC_SUCCESS;
}

void stopServer(int signo) {
    server::Application& app = server::Application::get();
    app.stop();
}

int deinitializeServer(void) {
    server::Application& app = server::Application::get();
    return app.deinit();
}

} // extern "C"