#include <google/protobuf/stubs/common.h>
#include <spdlog/spdlog.h>
#include "error_handling.h"
#include "cxxopts.hpp"
#include <csignal>
#include "ipc.pb.h"
#include "ipc.h"

int main(int argc, char *argv[]) {
    cxxopts::Options options("Producer", "Application options:");
    options.add_options()
        ("address", "Host name to connect to the server", cxxopts::value<std::string>()->default_value("ipc-server"), "STR")
        ("port", "Port number to connect to the server", cxxopts::value<int>()->default_value("24737"), "PORT")
        ("l,logging", "Directory to save the logging file", cxxopts::value<std::string>()->default_value("./client_log_1"), "PATH")
        ("h,help", "Print usage");

    auto resultParser = options.parse(argc, argv);
    if (resultParser.count("help")) {
        printf("%s\n", options.help().c_str());
        return 0;
    }
    std::string loggingDir = resultParser["logging"].as<std::string>();
    if (loggingDir.empty() == true) {
        loggingDir = "./client_log_1/log.txt";
    } else {
        loggingDir += "/log.txt";
    }
    int result = initializeLogging(loggingDir.c_str());
    if (result != EC_SUCCESS) {
        deinitializeLogging();
        return result;
    }
    const char* address  = resultParser["address"].as<std::string>().c_str();
    if (address == nullptr || std::strlen(address) == 0) {
        spdlog::error("Invalid address provided");
        return EC_FAILURE;
    }
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    const int port = resultParser["port"].as<int>();
    std::signal(SIGINT, stopHandleClient);
    std::signal(SIGTERM, stopHandleClient);

    const int receiveTimeoutMs = 3000;
    const uint8_t execFunc = ExecFunFlags::ADD | ExecFunFlags::MULT | ExecFunFlags::CONCAT;

    result = clientInitialize(address, port, receiveTimeoutMs, execFunc);
    if (result == EC_SUCCESS) {
        result = clientStart();
        if (result != EC_SUCCESS) {
            spdlog::error("Failed to start the client application");
        }
    } else {
        spdlog::error("Failed to initialize the client application");
    }

    result = clientDeinitialize();
    if (result != EC_SUCCESS) {
        spdlog::error("Failed to deinitialize the client application");
    }

    result = deinitializeLogging();
    google::protobuf::ShutdownProtobufLibrary();
    return result;
}
