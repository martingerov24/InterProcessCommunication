#include "ipc.h"
#include "error_handling.h"
#include "server/application.h"
#include "client/application.h"

extern "C" {

int server_initialize(const char* address, int port) {
    int result = server::Application::create(address, port);
    ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to create the server application");

    server::Application& app = server::Application::get();
    result = app.init();
    ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to initialize the server application");

    return EC_SUCCESS;
}

int server_run(void) {
    server::Application& app = server::Application::get();
    int result = app.run();
    ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to start the server application");
    return EC_SUCCESS;
}

void server_stop(int signo) {
    server::Application& app = server::Application::get();
    app.stop();
}

int server_deinitialize(void) {
    server::Application& app = server::Application::get();
    return app.deinit();
}

int client_initialize(const char* address, int port) {
    int result = client::Application::create(address, port);
    ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to create the server application");

    client::Application& app = client::Application::get();
    result = app.init();
    ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to initialize the server application");

    return EC_SUCCESS;
}

int client_deinitialize(void) {
    client::Application& app = client::Application::get();
    return app.deinit();
}

} // extern "C"