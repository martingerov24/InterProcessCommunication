#include "cxxopts.hpp"
#include "error_handling.h"
#include <csignal>
#include <spdlog/spdlog.h>
#include "spdlog/sinks/rotating_file_sink.h"
#include <google/protobuf/stubs/common.h>
#include "ipc.h"

int main(int argc, char *argv[]) {
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    cxxopts::Options options("Producer", "Application options:");
    options.add_options()
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

    // Create a file rotating logger with 50 MB size max and 2 rotated files
    int32_t max_size = 1048576 * 50;
    int32_t max_files = 2;
    std::shared_ptr<spdlog::logger> logger = spdlog::rotating_logger_mt("Producer", loggingDir, max_size, max_files);
    logger->flush_on(spdlog::level::info);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
    spdlog::info("\n\nSTART");

    std::signal(SIGINT, stopHandleServer);
    std::signal(SIGTERM, stopHandleServer);

    const int threads = resultParser["threads"].as<int>();
    int result = serverInitialize("0.0.0.0", 24737, threads);
    ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to initialize the server application");

    result = serverRun();
    ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to start the server application");

    result = serverDeinitialize();
    ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to deinitialize the server application");

    spdlog::info("END LOGGING");
    logger->flush();
    spdlog::shutdown();
    google::protobuf::ShutdownProtobufLibrary();
    return EC_SUCCESS;
}
