#include <google/protobuf/stubs/common.h>
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
using fnInitializeLogging = int (*)(const char *);
using fnDeinitializeLogging = int (*)(void);

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

    const std::string soPath = resultParser["so_path"].as<std::string>();
    if (fs::exists(soPath) == false) {
        printf("The shared object file does not exist: %s\n", soPath.c_str());
        return EC_FAILURE;
    }
    void* handle = dlopen(soPath.c_str(), RTLD_NOW);
    if (handle == nullptr) {
        printf("dlopen(%s) failed: %s\n", soPath.c_str(), dlerror());
        return EC_FAILURE;
    }
    auto clientInitialize = mustSym<fnClientInitialize>(handle, "clientInitialize");
    auto clientStart = mustSym<fnClientStart>(handle, "clientStart");
    auto clientDeinitialize = mustSym<fnClientDeinitialize>(handle, "clientDeinitialize");
    auto stopHandle = mustSym<fnStopHandle>(handle, "stopHandleClient");
    auto initLoggingFunc = mustSym<fnInitializeLogging>(handle, "initializeLogging");
    auto deinitializeLogging = mustSym<fnDeinitializeLogging>(handle, "deinitializeLogging");

    int result = initLoggingFunc(loggingDir.c_str());
    if (result != EC_SUCCESS) {
        dlclose(handle);
        return result;
    }
    const char* address  = resultParser["address"].as<std::string>().c_str();
    if (address == nullptr || std::strlen(address) == 0) {
        spdlog::error("Invalid address provided");
        deinitializeLogging();
        dlclose(handle);
        return EC_FAILURE;
    }

    GOOGLE_PROTOBUF_VERIFY_VERSION;
    const int port = resultParser["port"].as<int>();
    std::signal(SIGINT, stopHandle);
    std::signal(SIGTERM, stopHandle);

    const int receiveTimeoutMs = 3000;
    const uint8_t execFunc = ExecFunFlags::SUB | ExecFunFlags::DIV | ExecFunFlags::FIND_START;

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

    if (handle) {
        dlclose(handle);
    }
    google::protobuf::ShutdownProtobufLibrary();
    return result;
}
