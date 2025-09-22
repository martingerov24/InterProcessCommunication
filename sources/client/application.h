#pragma once
#include <atomic>
#include "zmq.hpp"
#include "ipc.pb.h"
#include <vector>

namespace client {

    // The `Application` struct encapsulates the client-side logic. It's a singleton
    // designed to manage a single client's connection and interactions with a server.
    // The functions are not marked `const` because ZeroMQ's socket operations
    // (like `send` and `recv`) modify the socket state internally.
    struct Application {
    private:
        // Helper function to send the initial handshake message to the server. Which tells the server which functions
        // Can be requested by this client.
        // This message contains the client's unique identity and its execution capabilities.
        int sendFirstHandshake();

        int sendEnvelope(const ipc::EnvelopeReq& env);

        int recvEnvelope(ipc::EnvelopeResp& out);

        // Private constructor to enforce the singleton pattern.
        explicit Application(
            const std::atomic<bool>& sigStop,
            const char* endpoint,
            const int port,
            const int receiveTimeoutMs,
            const uint8_t execFunFlags
        );

    public:
        // Static method to get the single instance of the Application.
        // Asserts that the instance has already been created.
        static Application& get();

        // Static method to create the single instance of the Application.
        // This is the only way to instantiate the class and is safe to call once.
        static int create(
            const std::atomic<bool>& sigStop,
            const char* address,
            const int port,
            const int receiveTimeoutMs,
            const uint8_t execFunFlags
        ) noexcept;

        // Destructor. Responsible for cleaning up resources, such as the ZeroMQ socket.
        ~Application();

        // Initializes the client, setting up the ZeroMQ socket and connecting to the server.
        int init();

        // The main client loop. This function runs the interactive command-line interface.
        int run();

        // Deinitializes the client, safely closing the socket.
        int deinit();

        // Submits a blocking request to the server. The function will wait for a
        // response from the server before returning.
        int submitBlocking(
            const ipc::SubmitRequest& req,
            ipc::SubmitResponse& out
        );

        // Submits a non-blocking request to the server. The server will respond
        // immediately with a ticket ID, and the actual result must be retrieved later.
        int submitNonBlocking(
            const ipc::SubmitRequest& req,
            ipc::SubmitResponse& out
        );

        // Retrieves the result for a previously submitted non-blocking request using its ticket ID.
        // Supports different waiting modes (e.g., no wait, wait up to a timeout).
        int getResult(
            const ipc::Ticket& ticket,
            const ipc::GetWaitMode waitMode,
            const uint32_t timeoutMs,
            ipc::GetResponse& out
        );

    private:
        zmq::context_t mCtx;                     // The ZeroMQ context for the client.
        zmq::socket_t mSocket;                   // The main ZeroMQ socket for communication.
        const std::string mIdentity;             // A unique, randomly generated ID for the client.
        const char* mEndpoint;                   // The server's address.
        const int mReceiveTimeoutMs;             // The timeout for receiving messages.
        const int mPort;                         // The server's port.
        const uint8_t mExecFunFlags;             // The bitmask of functions the client can perform.
        const std::atomic<bool>& mSigStop;       // A reference to a flag for graceful shutdown.
    };
} // namespace client