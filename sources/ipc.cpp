#include "ipc.h"
#include "error_handling.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"

extern "C" {
    bool verifyExecCaps(const uint8_t execFunFlags) {
        const uint8_t allExecFunFlags =
            ExecFunFlags::ADD |
            ExecFunFlags::SUB |
            ExecFunFlags::MULT |
            ExecFunFlags::DIV |
            ExecFunFlags::CONCAT |
            ExecFunFlags::FIND_START;
        return (execFunFlags & ~allExecFunFlags) == 0 && execFunFlags > 0;
    }

    int initializeLogging(const char* loggingDir) {
        try {
            // Create a rotating default logger: 50 MiB max, keep 2 files
            const std::size_t max_size  = 50u * 1024u * 1024u;
            const std::size_t max_files = 2;

            auto logger = spdlog::rotating_logger_mt("Producer", loggingDir, max_size, max_files);
            spdlog::set_default_logger(logger);
            spdlog::set_level(spdlog::level::info);
            logger->flush_on(spdlog::level::info);

            spdlog::info("START");
            return EC_SUCCESS;
        } catch (const std::exception& e) {
            fprintf(stderr, "Failed to init logging: %s\n", e.what());
            return EC_FAILURE;
        }
    }

    int deinitializeLogging() {
        if (auto logger = spdlog::default_logger()) {
            logger->info("Shutting down...");
            logger->flush();
        }
        spdlog::shutdown();
        return EC_SUCCESS;
    }
}
