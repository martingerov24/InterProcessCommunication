#include <google/protobuf/stubs/common.h>
#include "error_handling.h"
#include <spdlog/spdlog.h>
#include "cxxopts.hpp"
#include <csignal>
#include "ipc.h"

int main(int argc, char *argv[]) {
    cxxopts::Options options("Producer", "Application options:");
    options.add_options()
        ("port", "Port number to connect to the server", cxxopts::value<int>()->default_value("24737"), "PORT")
        ("l,logging", "Directory to save the logging file", cxxopts::value<std::string>()->default_value("./server_log"), "PATH")
        ("threads", "Number of worker threads", cxxopts::value<int>()->default_value("4"), "INT")
        ("h,help", "Print usage");

    auto resultParser = options.parse(argc, argv);
    if (resultParser.count("help")) {
        printf("%s\n", options.help().c_str());
        return 0;
    }
    std::string loggingDir = resultParser["logging"].as<std::string>();
    if (loggingDir.empty()) {
        loggingDir = "./log/log.txt";
    } else {
        loggingDir += "/log.txt";
    }
    int result = initializeLogging(loggingDir.c_str());
    if (result != EC_SUCCESS) {
        return result;
    }

    GOOGLE_PROTOBUF_VERIFY_VERSION;
    std::signal(SIGINT, stopHandleServer);
    std::signal(SIGTERM, stopHandleServer);

    const int threads = resultParser["threads"].as<int>();
    const int port = resultParser["port"].as<int>();

    result = serverInitialize("0.0.0.0", port, threads);
    if (result == EC_SUCCESS) {
        result = serverRun();
        if (result != EC_SUCCESS) {
            spdlog::error("Failed to run the server application");
        }
    } else {
        spdlog::error("Failed to initialize the server application");
    }
    result = serverDeinitialize();
    if (result != EC_SUCCESS) {
        spdlog::error("Failed to deinitialize the server application");
    }

    result = deinitializeLogging();
    google::protobuf::ShutdownProtobufLibrary();
    return result;
}
