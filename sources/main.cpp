#include "cxxopts.hpp"
#include "error_handling.h"
#include <csignal>
#include <spdlog/spdlog.h>
#include "spdlog/sinks/rotating_file_sink.h"
#include "ipc.pb.h"
#include "ipc.h"

static ipc::SubmitRequest makeMath(
    const ipc::MathOp op,
    const int32_t a,
    const int32_t b
) {
    ipc::SubmitRequest s;
    auto* m = s.mutable_math();
    m->set_op(op);
    m->set_a(a);
    m->set_b(b);
    return s;
}

static ipc::SubmitRequest makeStr(
    const ipc::StrOp op,
    const std::string& s1,
    const std::string& s2
) {
    ipc::SubmitRequest s;
    auto* st = s.mutable_str();
    st->set_op(op);
    st->set_s1(s1);
    st->set_s2(s2);
    return s;
}

int main(int argc, char *argv[]) {
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    cxxopts::Options options("Producer", "Application options:");
    options.add_options()
        ("t,type", "Server or Client", cxxopts::value<std::string>()->default_value("server"), "string")
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
    if (type != "server" && type != "client") {
        spdlog::error("Invalid type: {}", type);
        return -1;
    }
    spdlog::info("Type: {}", type);
    if (type == "server") {
        std::signal(SIGINT, server_stop);
        std::signal(SIGTERM, server_stop);
        int result = server_initialize("localhost", 5555);
        ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to initialize the server application");
        result = server_run();
        ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to start the server application");
        result = server_deinitialize();
        ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to deinitialize the server application");
    } else if ("client") {
        int result = client_initialize("localhost", 5555);
        ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to initialize the client application");

        result = client_deinitialize();
        ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to deinitialize the client application");
    } else {
        spdlog::error("Unknown type: {}", type);
        return -1;
    }

    spdlog::info("END LOGGING");
    logger->flush();
    spdlog::shutdown();
    google::protobuf::ShutdownProtobufLibrary();
    return EC_SUCCESS;
}
