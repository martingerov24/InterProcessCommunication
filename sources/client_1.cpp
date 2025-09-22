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

    std::signal(SIGINT, stopHandleClient);
    std::signal(SIGTERM, stopHandleClient);

    const int receiveTimeoutMs = 3000;
    const uint8_t execFunc = ExecFunFlags::ADD | ExecFunFlags::MULT | ExecFunFlags::CONCAT;

    int result = clientInitialize("127.0.0.1", 24737, receiveTimeoutMs, execFunc);
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
