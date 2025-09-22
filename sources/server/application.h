#pragma once
#include <atomic>
#include "zmq.hpp"
#include <vector>
#include "algorithm_runner.h" // The header for the AlgoRunner, which performs computational tasks.

namespace server {
    /// @brief A singleton class representing the server application.
    ///
    /// This class manages the entire server lifecycle: initialization,
    /// running the main message loop, and deinitialization.
    struct Application {
    private:
        /// @brief Handles an incoming client request encapsulated in an Envelope.
        ///
        /// This method is responsible for routing the request to the appropriate
        /// handler (e.g., AlgoRunner) and preparing the response. It also
        /// checks if the client has the necessary execution capabilities.
        /// @param request The incoming request message.
        /// @param clientExecCaps A bitmask of the client's execution capabilities.
        /// @param response The outgoing response message.
        /// @return An error code, 0 for success.
        int handleEnvelope(
            const ipc::EnvelopeReq& request,
            const uint8_t clientExecCaps,
            ipc::EnvelopeResp& response
        ) const;

        /// @brief Private constructor to enforce the singleton pattern.
        ///
        /// It's `explicit` to prevent implicit conversions and `noexcept`
        /// as it's a simple initialization list.
        explicit Application(
            const std::atomic<bool>& sigStop,
            const char* address,
            const int port,
            const int theads
        ) noexcept;

        // Disabling copy constructor and assignment operator to enforce singleton.
        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

    public:
        /// @brief Static method to get the single instance of the Application.
        /// @return A reference to the singleton instance. Asserts that the instance exists.
        static Application& get();

        /// @brief Static method to create the single instance of the Application.
        ///
        /// This is the only way to create the Application object. It prevents
        /// multiple instances from being created.
        /// @param sigStop An atomic boolean flag to signal the application to stop.
        /// @param address The network address to bind to.
        /// @param port The port number to bind to.
        /// @param threads The number of threads for the algorithm runner's pool.
        /// @return An error code, 0 for success.
        static int create(
            const std::atomic<bool>& sigStop,
            const char* address,
            const int port,
            const int threads
        ) noexcept;

        /// @brief Initializes the server, including its ZeroMQ socket and algorithm runner. In this case the Socket is a ROUTER.
        /// @return An error code, 0 for success.
        int init();

        /// @brief Starts the server's main loop (a blocking call).
        ///
        /// The server listens for and handles incoming requests until the `mSigStop`
        /// flag is set.
        /// @return An error code, 0 for success.
        int run();

        /// @brief Deinitializes the server, closing the socket and cleaning up resources.
        /// @return An error code, 0 for success.
        int deinit();

        /// @brief Destructor. Ensures cleanup if `deinit()` was not called.
        ~Application();

    private:
        zmq::context_t mCtx{1};                     ///< The ZeroMQ context for the application.
        zmq::socket_t mRouter{mCtx, zmq::socket_type::router}; ///< The main ZeroMQ ROUTER socket for IPC.
        std::unordered_map<std::string, uint8_t> mClientExecCaps; ///< Stores client capabilities indexed by client ID.
        AlgoRunner mAlgoRunner;                     ///< The component for running computational algorithms.
        const char* mAddress;                       ///< The network address the server is bound to.
        const int mPort;                            ///< The port number the server is bound to.
        const int mThreads;                         ///< The number of threads for the AlgoRunner.
        const std::atomic<bool>& mSigStop;          ///< Reference to the external stop signal flag.
        std::atomic<bool> mInitialized{false};      ///< A flag to track the initialization state of the application.
    };
} // namespace server