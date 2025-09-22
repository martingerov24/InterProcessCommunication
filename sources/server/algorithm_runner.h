#pragma once
#include "ipc.pb.h"
#include <memory> //Used for std::unique_ptr.

// The server namespace encapsulates all related server-side code.
namespace server {

    // Forward declaration of the private implementation struct.
    struct AlgoRunnerIpml;

    // The public interface for the algorithm runner.
    // It's a "handle" class that delegates all its work to an internal implementation object.
    struct AlgoRunner {

        /// @brief Initializes the AlgoRunner and its internal thread pool.
        /// @param threads The number of threads to create for the thread pool.
        /// @return An error code; 0 for success.
        int init(const int threads);

        /// @brief Deinitializes the AlgoRunner, stopping all threads and cleaning up resources.
        /// @return An error code; 0 for success.
        int deinit();

        /// @brief Submits a computational request for execution.
        /// @param request A Protocol Buffer message containing the request details (e.g., math or string operation).
        /// If a non-blocking operation is requested, a ticket ID will be generated and returned in the response.
        /// If a blocking operation is requested, the result will be computed and returned in the response.
        /// @param response A Protocol Buffer message where the result of the request will be stored.
        /// @return An error code; 0 for success.
        int run(
            const ipc::SubmitRequest& request,
            ipc::SubmitResponse& response
        ) const;

        /// @brief Retrieves the result of a previously submitted non-blocking request.
        /// @param request A Protocol Buffer message containing the ticket ID of the request to retrieve.
        /// @param response A Protocol Buffer message where the result will be stored.
        /// @return An error code; 0 for success.
        int get(
            const ipc::GetRequest& request,
            ipc::GetResponse& response
        ) const ;

    private:
        // The implementation is defined in the .cpp file.
        std::unique_ptr<AlgoRunnerIpml>* outImpl = nullptr;
    };
};