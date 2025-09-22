#include "spdlog/sinks/rotating_file_sink.h"
#include "common/error_handling.h"
#include "error_handling.h"
#include <spdlog/spdlog.h>
#include "cxxopts.hpp"
#include "ipc.pb.h"
#include <csignal>
#include <dlfcn.h>
#include <cstdlib>
#include "ipc.h"

#if defined(__has_include)
#if __has_include(<filesystem>)
#    include <filesystem>
namespace fs = std::filesystem;
#else
#    include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif
#else
#    include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif


using fnClientInitialize = int (*)(const char*, const int, const int, const uint8_t);
using fnClientStart = int (*)(void);
using fnClientDeinitialize = int (*)(void);
using fnStopHandle = void (*)(int);

static void* mustOpen(const char* so_path) {
    void* h = dlopen(so_path, RTLD_NOW);
    if (!h) {
        std::fprintf(stderr, "dlopen(%s) failed: %s\n", so_path, dlerror());
        std::exit(1);
    }
    return h;
}

template<typename T>
static T mustSym(void* h, const char* name) {
    dlerror();
    void* p = dlsym(h, name);
    if (const char* e = dlerror()) {
        dlclose(h);
        std::fprintf(stderr, "dlsym(%s) failed: %s\n", name, e);
        std::exit(1);
    }
    return reinterpret_cast<T>(p);
}

int main(int argc, char *argv[]) {
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    cxxopts::Options options("Producer", "Application options:");
    options.add_options()
        ("address", "Host name to connect to the server", cxxopts::value<std::string>()->default_value("ipc-server"), "STR")
        ("port", "Port number to connect to the server", cxxopts::value<int>()->default_value("24737"), "PORT")
        ("so_path", "Path to the shared object file", cxxopts::value<std::string>()->default_value("./libclientipc.so"), "PATH")
        ("l,logging", "Directory to save the logging file", cxxopts::value<std::string>()->default_value("./client_log_2"), "PATH")
        ("h,help", "Print usage");

    auto resultParser = options.parse(argc, argv);
    if (resultParser.count("help")) {
        printf("%s\n", options.help().c_str());
        return 0;
    }
    std::string loggingDir = resultParser["logging"].as<std::string>();
    if (loggingDir.empty() == true) {
        loggingDir = "./client_log_2/log.txt";
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

    const std::string soPath = resultParser["so_path"].as<std::string>();
    if (fs::exists(soPath) == false) {
        spdlog::error("Shared object file does not exist: {}", soPath);
        return EC_FAILURE;
    }

    void* handle = mustOpen(soPath.c_str());
    auto clientInitialize = mustSym<fnClientInitialize>(handle, "clientInitialize");
    auto clientStart = mustSym<fnClientStart>(handle, "clientStart");
    auto clientDeinitialize = mustSym<fnClientDeinitialize>(handle, "clientDeinitialize");
    auto stopHandle = mustSym<fnStopHandle>(handle, "stopHandleClient");

    const char* address  = resultParser["address"].as<std::string>().c_str();
    if (address == nullptr || std::strlen(address) == 0) {
        spdlog::error("Invalid address provided");
        dlclose(handle);
        return EC_FAILURE;
    }
    const int port = resultParser["port"].as<int>();

    std::signal(SIGINT, stopHandle);
    std::signal(SIGTERM, stopHandle);

    const int receiveTimeoutMs = 3000;
    const uint8_t execFunc = ExecFunFlags::SUB | ExecFunFlags::DIV | ExecFunFlags::FIND_START;

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
    dlclose(handle);
    return EC_SUCCESS;
}
