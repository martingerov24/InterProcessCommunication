#include "cxxopts.hpp"
#include "application.h"
#include "error_handling.h"
#include <spdlog/spdlog.h>
#include "spdlog/sinks/rotating_file_sink.h"

int main(int argc, char *argv[]) {
    cxxopts::Options options("Producer", "Application options:");
    options.add_options()
        ("t,type", "Server or Consumer", cxxopts::value<std::string>()->default_value("server"), "string")
        ("l,logging", "Directory to save the logging file", cxxopts::value<std::string>()->default_value("./log"), "PATH")
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


    std::string type = resultParser["type"].as<std::string>();
    if (type != "server" && type != "consumer") {
        spdlog::error("Invalid type: {}", type);
        return -1;
    }
    spdlog::info("Type: {}", type);
    int result = EC_SUCCESS;
    if (type == "server") {
        server::Application app;
        // int init(const char* address, const int port) {
        result = app.init("127.0.0.1", 5555);
        ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to initialize the server application");
        result = app.run();
        ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to run the server application");
        result = app.deinit();
        ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to deinitialize the server application");
    } else {
        spdlog::info("Consumer mode is not implemented yet.");
        return -1;
    }

    spdlog::info("END LOGGING");
    logger->flush();
    spdlog::shutdown();
    return result;
}
