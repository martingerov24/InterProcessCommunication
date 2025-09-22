#include "spdlog/sinks/rotating_file_sink.h"
#include <spdlog/spdlog.h>
#include "error_handling.h"
#include "cxxopts.hpp"
#include <csignal>
#include "ipc.pb.h"
#include "ipc.h"

int main(int argc, char *argv[]) {
    GOOGLE_PROTOBUF_VERIFY_VERSION;
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

    // Create a file rotating logger with 50 MB size max and 2 rotated files
    int32_t max_size = 1048576 * 50;
    int32_t max_files = 2;
    std::shared_ptr<spdlog::logger> logger = spdlog::rotating_logger_mt("Producer", loggingDir, max_size, max_files);
    logger->flush_on(spdlog::level::info);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
    spdlog::info("\n\nSTART");

    const char* address  = resultParser["address"].as<std::string>().c_str();
    if (address == nullptr || std::strlen(address) == 0) {
        spdlog::error("Invalid address provided");
        return EC_FAILURE;
    }
    const int port = resultParser["port"].as<int>();

    std::signal(SIGINT, stopHandleClient);
    std::signal(SIGTERM, stopHandleClient);

    const int receiveTimeoutMs = 3000;
    const uint8_t execFunc = ExecFunFlags::ADD | ExecFunFlags::MULT | ExecFunFlags::CONCAT;

    int result = clientInitialize(address, port, receiveTimeoutMs, execFunc);
    ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to initialize the client application");

    result = clientStart();
    ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to start the client application");

    result = clientDeinitialize();
    ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to deinitialize the client application");

    spdlog::info("END LOGGING");
    logger->flush();
    spdlog::shutdown();
    google::protobuf::ShutdownProtobufLibrary();
    return EC_SUCCESS;
}
