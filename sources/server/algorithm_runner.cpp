#include "algorithm_runner.h"
#include "error_handling.h"
#include <functional>
#include <spdlog/spdlog.h>

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <deque>

using namespace server;

namespace server {
    struct AlgoRunnerIpml {
    private:
        struct Job {
            uint64_t id = 0;
            ipc::SubmitRequest req;
            ipc::Status status = ipc::ST_NOT_FINISHED;
            ipc::Result result;
            std::mutex m;
            std::condition_variable cv;
            bool done = false;
        };

        ipc::Status runMath(
            const ipc::MathArgs& request,
            ipc::Result& response
        ) const;

        ipc::Status runStr(
            const ipc::StrArgs& request,
            ipc::Result& response
        ) const;

        void workerLoop();

        uint64_t enqueue(const ipc::SubmitRequest& req);

        std::shared_ptr<Job> findJobById(uint64_t id);
    public:
        AlgoRunnerIpml(const int threads);

        int init();

        int deinit();

        int run(
            const ipc::SubmitRequest& request,
            ipc::SubmitResponse& response
        );

        int get(
            const ipc::GetRequest& request,
            ipc::GetResponse& response
        );
    private:
        std::mutex jobsMtx;
        std::unordered_map<uint64_t, std::shared_ptr<Job>> jobs;

        std::mutex qMtx;
        std::condition_variable qCv;
        std::deque<std::shared_ptr<Job>> jobQueue;
        std::vector<std::thread> workers;

        std::atomic<uint64_t> nextId{1};
        const int maxThreads;
        std::atomic<bool> running{false};
    };
};

AlgoRunnerIpml::AlgoRunnerIpml(const int threads)
: maxThreads(threads) {}

// PUBLIC CLASS METHODS
int AlgoRunner::init(const int threads) {
    if (outImpl != nullptr) {
        spdlog::error("AlgoRunner is already initialized");
        return EC_SUCCESS;
    }
    outImpl = new std::unique_ptr<AlgoRunnerIpml>(new AlgoRunnerIpml(threads));
    return (*outImpl)->init();
}

int AlgoRunner::deinit() {
    if (outImpl == nullptr) {
        spdlog::error("AlgoRunner is not initialized");
        return EC_SUCCESS;
    }
    int result = (*outImpl)->deinit();
    delete outImpl;
    outImpl = nullptr;
    return result;
}

int AlgoRunner::run(
    const ipc::SubmitRequest& request,
    ipc::SubmitResponse& response
) const {
    if (outImpl == nullptr) {
        spdlog::error("AlgoRunner is not initialized");
        return EC_FAILURE;
    }
    return (*outImpl)->run(request, response);
}

int AlgoRunner::get(
    const ipc::GetRequest& request,
    ipc::GetResponse& response
) const {
    if (outImpl == nullptr) {
        spdlog::error("AlgoRunner is not initialized");
        return EC_FAILURE;
    }
    return (*outImpl)->get(request, response);
}
// ~ PUBLIC CLASS METHODS

// PRIVATE CLASS METHODS
ipc::Status AlgoRunnerIpml::runMath(
    const ipc::MathArgs& request,
    ipc::Result& response
) const {
    switch (request.op()) {
    case ipc::MATH_ADD:
        response.set_int_result(request.a() + request.b());
        break;
    case ipc::MATH_SUB:
        response.set_int_result(request.a() - request.b());
        break;
    case ipc::MATH_MUL:
        response.set_int_result(request.a() * request.b());
        break;
    case ipc::MATH_DIV:
        if (request.b() == 0) {
            return ipc::ST_ERROR_DIV_BY_ZERO;
        }
        response.set_int_result(request.a() / request.b());
        break;
    default:
        return ipc::ST_ERROR_INVALID_INPUT;
    }
    return ipc::ST_SUCCESS;
}

ipc::Status AlgoRunnerIpml::runStr(
    const ipc::StrArgs& request,
    ipc::Result& response
) const {
    if (request.op() == ipc::STR_CONCAT) {
        std::string r = request.s1() + request.s2();
        if (r.size() > 32) {
            return ipc::Status::ST_ERROR_STRING_TOO_LONG;
        }
        response.set_str_result(r);
    } else if (request.op() == ipc::STR_FIND_START) {
        std::size_t pos = request.s1().find(request.s2());
        if (pos == std::string::npos) {
            return ipc::ST_ERROR_SUBSTR_NOT_FOUND;
        }
        response.set_position(static_cast<int32_t>(pos));
    } else {
        return ipc::ST_ERROR_INVALID_INPUT;
    }
    return ipc::Status::ST_SUCCESS;
}

void AlgoRunnerIpml::workerLoop() {
    while(running.load()) {
        std::shared_ptr<AlgoRunnerIpml::Job> job;
        {
            std::unique_lock<std::mutex> lk(qMtx);
            qCv.wait(lk, [&] { return running.load() == false || jobQueue.empty() == false; });
            if (running.load() == false && jobQueue.empty()) {
                break;
            }
            job = std::move(jobQueue.front());
            jobQueue.pop_front();
        }

        ipc::Result result;
        ipc::Status status = ipc::ST_ERROR_INVALID_INPUT;

        if (job->req.has_math()) {
            status = runMath(job->req.math(), result);
        } else if (job->req.has_str()) {
            status = runStr(job->req.str(), result);
        } else {
            status = ipc::ST_ERROR_INVALID_INPUT;
        }

        {
            std::lock_guard<std::mutex> g(job->m);
            job->status = status;
            job->result.Swap(&result);
            job->done = true;
        }
        job->cv.notify_all();
    }
}

uint64_t AlgoRunnerIpml::enqueue(const ipc::SubmitRequest& req) {
    using namespace std::chrono;

    std::shared_ptr<Job> job = std::make_shared<Job>();
    static std::atomic<uint64_t> seq{0};
    auto now = steady_clock::now();
    uint64_t ts = duration_cast<nanoseconds>(now.time_since_epoch()).count();
    uint64_t id = (ts << 16) | (seq.fetch_add(1) & 0xFFFF);
    job->id = id;
    job->req = req;

    {
        std::lock_guard<std::mutex> lk(jobsMtx);
        jobs[id] = job;
    }
    {
        std::lock_guard<std::mutex> lk(qMtx);
        jobQueue.emplace_back(std::move(job));
    }
    qCv.notify_one();
    return id;
}

std::shared_ptr<AlgoRunnerIpml::Job> AlgoRunnerIpml::findJobById(uint64_t id) {
    std::lock_guard<std::mutex> lk(jobsMtx);
    auto it = jobs.find(id);
    return it == jobs.end() ? nullptr : it->second;
}

int AlgoRunnerIpml::init() {
    if (running.load()) {
        return EC_SUCCESS;
    }
    running.store(true);

    workers.reserve(maxThreads);
    for (int i = 0; i < maxThreads; ++i) {
        workers.emplace_back(
            [this] {
                workerLoop();
            }
        );
    }
    return EC_SUCCESS;
}

int AlgoRunnerIpml::deinit() {
    if (running.load() == false) {
        return EC_SUCCESS;
    }
    running.store(false);
    {
        std::lock_guard<std::mutex> lk(qMtx);
    }
    qCv.notify_all();
    for (std::thread& thead : workers) {
        if (thead.joinable()) {
            thead.join();
        }
    }
    workers.clear();
    return EC_SUCCESS;
}

int AlgoRunnerIpml::run(
    const ipc::SubmitRequest& request,
    ipc::SubmitResponse& response
) {
    const ipc::SubmitMode mode = request.mode();
    if (mode == ipc::SubmitMode::BLOCKING) {
        ipc::Status result = ipc::ST_SUCCESS;
        ipc::Result* out = response.mutable_result();
        if (request.has_math()) {
            result = runMath(request.math(), *out);
            response.set_status(result);
            PRINT_ERROR_NO_RET(ErrorType::IPC, result, "Failed to run math operation");
        } else if (request.has_str()) {
            result = runStr(request.str(), *out);
            response.set_status(result);
            PRINT_ERROR_NO_RET(ErrorType::IPC, result, "Failed to run string operation");
        } else {
            response.set_status(ipc::ST_ERROR_INVALID_INPUT);
        }
        return EC_SUCCESS;
    }
    if (mode == ipc::SubmitMode::NONBLOCKING) {
        if (request.has_math() == false && request.has_str() == false) {
            response.set_status(ipc::ST_ERROR_INVALID_INPUT);
            return EC_SUCCESS;
        }
        const uint64_t id = enqueue(request);
        response.set_status(ipc::ST_NOT_FINISHED);

        response.mutable_ticket()->set_req_id(id);
        return EC_SUCCESS;
    }
    response.set_status(ipc::ST_ERROR_INVALID_INPUT);
    return EC_SUCCESS;
}

int AlgoRunnerIpml::get(
    const ipc::GetRequest& request,
    ipc::GetResponse& response
) {
    const uint64_t id = request.ticket().req_id();
    std::shared_ptr<server::AlgoRunnerIpml::Job> job = findJobById(id);
    if (job == nullptr) {
        response.set_status(ipc::ST_ERROR_INVALID_INPUT);
        return EC_SUCCESS;
    }

    if (request.wait_mode() == ipc::NO_WAIT) {
        std::lock_guard<std::mutex> g(job->m);
        if (job->done == false) {
            response.set_status(ipc::ST_NOT_FINISHED);
            return EC_SUCCESS;
        }
        response.set_status(job->status);
        response.mutable_result()->Swap(&job->result);
        {
            std::lock_guard<std::mutex> jl(jobsMtx);
            jobs.erase(id);
        }
        return EC_SUCCESS;
    }

    if (request.wait_mode() == ipc::WAIT_UP_TO) {
        const uint32_t ms = request.timeout_ms();
        std::unique_lock<std::mutex> lk(job->m);
        if (job->cv.wait_for(lk, std::chrono::milliseconds(ms), [&] { return job->done; }) == false) {
            response.set_status(ipc::ST_NOT_FINISHED);
            return EC_SUCCESS;
        }
        response.set_status(job->status);
        response.mutable_result()->Swap(&job->result);
        {
            std::lock_guard<std::mutex> jl(jobsMtx);
            jobs.erase(id);
        }
        return EC_SUCCESS;
    }

    response.set_status(ipc::ST_ERROR_INVALID_INPUT);
    return EC_SUCCESS;
}
// ~ PRIVATE CLASS METHODS