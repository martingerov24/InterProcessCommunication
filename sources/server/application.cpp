#include "application.h"
#include <zmq.hpp>
#include "signal.h"
#include "error_handling.h"
#include <spdlog/spdlog.h>

static void signalHandler(int);

namespace server {

    struct ApplicationImpl {
    private:
        ApplicationImpl() {
            signal(SIGINT, signalHandler);
            signal(SIGTERM, signalHandler);
        }

    public:
        static ApplicationImpl& get() noexcept {
            assert(appPtr != nullptr);
            return *appPtr;
        }
        static int create() noexcept {
            static int instanceCount = 0;
            if (instanceCount >= 1) {
                spdlog::error("Only one instance of Application is allowed");
                return EC_FAILURE;
            }
            instanceCount++;
            appPtr = std::unique_ptr<ApplicationImpl>(new ApplicationImpl());
            return EC_SUCCESS;
        }

        int init(const char* address, const int port) {
            endpoint = std::string("tcp://") + address + ":" + std::to_string(port);
            return EC_SUCCESS;
        }

        int deinit() {
            return EC_SUCCESS;
        }

        int run() {
            spdlog::info("Server running at {}", endpoint);
            return EC_SUCCESS;
        }
    private:
        std::string endpoint;
        std::atomic<bool> running{false};

        // ZMQ context and ROUTER socket
        zmq::context_t ctx{1};
        zmq::socket_t  router{ctx, zmq::socket_type::router};
    };
} // namespace server

std::shared_ptr<server::ApplicationImpl> appPtr = nullptr;
using namespace server;

int Application::init(const char* address, int port) noexcept {
    int result = server::ApplicationImpl::create();
    ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to create Application instance");
    ApplicationImpl& mImpl = ApplicationImpl::get();
    result = mImpl.init(address, port);
    ERROR_CHECK(ErrorType::DEFAULT, result, "Failed to initialize Application");
    return EC_SUCCESS;
}

Application::~Application() {
    ApplicationImpl& mImpl = ApplicationImpl::get();
    int result = mImpl.deinit();

    ERROR_CHECK_NO_RET(ErrorType::DEFAULT, result, "Failed to deinitialize Application");
}

int Application::run() {
    ApplicationImpl& mImpl = ApplicationImpl::get();
    return mImpl.run();
}

// ---------- Signal handling ----------
void signalHandler(int) {
    spdlog::info("Signal caught, shutting down…");
    ApplicationImpl& application =  server::ApplicationImpl::get();
    int result = application.deinit();
    ERROR_CHECK_NO_RET(ErrorType::DEFAULT, result, "Failed to deinitialize Application in signal handler");
}
