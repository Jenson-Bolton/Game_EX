/**
 * @file job_system.cpp
 * @brief Implementation of bounded fixed-worker batch execution.
 */

#include "game_ex/jobs/job_system.hpp"

#include <algorithm>
#include <condition_variable>
#include <cstdlib>
#include <latch>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace game_ex::jobs {
namespace {

/**
 * @brief Complete prepared batch shared by its control and worker threads.
 */
struct PreparedBatch final {
    /**
     * @brief Copies all jobs and allocates all result storage before publication.
     * @param submitted_jobs Valid non-empty jobs no larger than the worker pool.
     */
    explicit PreparedBatch(const std::span<const Job> submitted_jobs)
        : jobs(submitted_jobs.begin(), submitted_jobs.end()),
          exceptions(jobs.size()),
          completion(static_cast<std::ptrdiff_t>(jobs.size())) {}

    /** Immutable callable storage prepared before workers can observe it. */
    const std::vector<Job> jobs;

    /** Per-position exception slots, each written by only one worker. */
    std::vector<std::exception_ptr> exceptions;

    /** Counts every normal or exceptional job completion. */
    std::latch completion;

    /** Next unclaimed job position, protected by the pool mutex. */
    std::size_t next_index{0};
};

} // namespace

/**
 * @brief Synchronization state and persistent threads hidden from public headers.
 */
class JobSystem::Implementation final {
public:
    /**
     * @brief Validates configuration and creates every persistent worker.
     * @param configuration Requested bounded worker count.
     */
    explicit Implementation(const JobSystemConfiguration configuration)
        : owner_thread_(std::this_thread::get_id()), worker_count_(configuration.worker_count) {
        if (worker_count_ == 0 || worker_count_ > maximum_worker_count) {
            throw std::invalid_argument("Job-system worker count must be between 1 and 32");
        }

        workers_.reserve(worker_count_);
        try {
            for (std::size_t index = 0; index < worker_count_; ++index) {
                static_cast<void>(index);
                workers_.emplace_back([this] { worker_loop(); });
            }
        } catch (...) {
            stop_and_join();
            throw;
        }
    }

    /** Ensures every successfully created worker is stopped and joined. */
    ~Implementation() {
        stop_and_join();
    }

    /** Implementation state is stable and cannot be copied. */
    Implementation(const Implementation&) = delete;

    /** Implementation state cannot be copy-assigned. */
    Implementation& operator=(const Implementation&) = delete;

    /**
     * @brief Publishes and synchronously settles one prepared batch.
     * @param jobs Validated jobs in deterministic caller order.
     * @return Complete ordered result vector.
     */
    [[nodiscard]] BatchResult execute_batch(const std::span<const Job> jobs) {
        require_owner_thread("execute a job batch");

        if (jobs.size() > worker_count_) {
            throw std::invalid_argument("Job batch cannot contain more jobs than workers");
        }
        for (const auto& job : jobs) {
            if (!job) {
                throw std::invalid_argument("Job batch cannot contain an empty callable");
            }
        }

        if (jobs.empty()) {
            std::scoped_lock lock(mutex_);
            require_accepting_locked();
            return BatchResult({});
        }

        auto prepared = std::make_shared<PreparedBatch>(jobs);
        {
            std::scoped_lock lock(mutex_);
            require_accepting_locked();
            if (active_batch_) {
                throw std::logic_error("A job batch is already active");
            }
            active_batch_ = prepared;
        }

        work_available_.notify_all();
        prepared->completion.wait();

        {
            std::scoped_lock lock(mutex_);
            active_batch_.reset();
        }

        return BatchResult(std::move(prepared->exceptions));
    }

    /**
     * @brief Performs creator-thread-checked explicit worker shutdown.
     */
    void shutdown() {
        require_owner_thread("shut down the job system");

        {
            std::scoped_lock lock(mutex_);
            if (state_ != JobSystemState::running) {
                throw std::logic_error("Job system shutdown has already begun");
            }
            if (active_batch_) {
                throw std::logic_error("Job system cannot shut down during an active batch");
            }
            state_ = JobSystemState::stopping;
        }

        work_available_.notify_all();
        join_workers();

        std::scoped_lock lock(mutex_);
        state_ = JobSystemState::stopped;
    }

    /**
     * @brief Returns the immutable worker count.
     * @return Number of persistent workers.
     */
    [[nodiscard]] std::size_t worker_count() const noexcept {
        return worker_count_;
    }

    /**
     * @brief Reads lifecycle state under synchronization.
     * @return Current job-system state.
     */
    [[nodiscard]] JobSystemState state() const noexcept {
        std::scoped_lock lock(mutex_);
        return state_;
    }

    /**
     * @brief Reports whether the caller is the stable control thread.
     * @return True only on the thread that constructed the pool.
     */
    [[nodiscard]] bool is_owner_thread() const noexcept {
        return std::this_thread::get_id() == owner_thread_;
    }

private:
    /**
     * @brief Rejects control operations outside the creating thread.
     * @param operation Human-readable operation used in diagnostics.
     */
    void require_owner_thread(const char* const operation) const {
        if (std::this_thread::get_id() != owner_thread_) {
            throw std::logic_error(std::string("Only the creating thread may ") + operation);
        }
    }

    /**
     * @brief Rejects batch publication unless the pool is running.
     * @throws std::logic_error when shutdown has begun.
     */
    void require_accepting_locked() const {
        if (state_ != JobSystemState::running) {
            throw std::logic_error("Job system does not accept work after shutdown begins");
        }
    }

    /** Waits for batches and invokes claimed jobs until shutdown is requested. */
    void worker_loop() noexcept {
        for (;;) {
            std::shared_ptr<PreparedBatch> batch;
            std::size_t index = 0;
            {
                std::unique_lock lock(mutex_);
                work_available_.wait(lock, [this] {
                    return state_ != JobSystemState::running
                        || (active_batch_ && active_batch_->next_index < active_batch_->jobs.size());
                });

                if (state_ != JobSystemState::running && !active_batch_) {
                    return;
                }

                if (!active_batch_ || active_batch_->next_index >= active_batch_->jobs.size()) {
                    continue;
                }

                batch = active_batch_;
                index = batch->next_index;
                ++batch->next_index;
            }

            try {
                batch->jobs[index]();
            } catch (...) {
                batch->exceptions[index] = std::current_exception();
            }
            batch->completion.count_down();
        }
    }

    /**
     * @brief Requests worker termination and joins every joinable thread.
     *
     * Used during both ordinary destruction and partial constructor failure.
     */
    void stop_and_join() noexcept {
        {
            std::scoped_lock lock(mutex_);
            if (state_ == JobSystemState::stopped) {
                return;
            }
            state_ = JobSystemState::stopping;
        }

        work_available_.notify_all();
        join_workers();

        std::scoped_lock lock(mutex_);
        state_ = JobSystemState::stopped;
    }

    /** Joins every successfully created persistent worker. */
    void join_workers() noexcept {
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    /** Thread that constructed the pool and owns its control operations. */
    const std::thread::id owner_thread_;

    /** Immutable validated size of the fixed worker pool. */
    const std::size_t worker_count_;

    /** Protects lifecycle, active batch publication, and work claiming. */
    mutable std::mutex mutex_;

    /** Wakes persistent workers for batch publication or shutdown. */
    std::condition_variable work_available_;

    /** Every persistent worker, including those created before constructor failure. */
    std::vector<std::thread> workers_;

    /** Published batch, reset only after its completion latch reaches zero. */
    std::shared_ptr<PreparedBatch> active_batch_;

    /** Synchronized pool lifecycle. */
    JobSystemState state_{JobSystemState::running};
};

std::size_t recommended_worker_count() noexcept {
    const unsigned int reported_threads = std::thread::hardware_concurrency();
    if (reported_threads <= 1U) {
        return 1;
    }

    const std::size_t available_workers = static_cast<std::size_t>(reported_threads - 1U);
    return std::min(available_workers, maximum_recommended_worker_count);
}

BatchResult::BatchResult(std::vector<std::exception_ptr> exceptions) noexcept
    : exceptions_(std::move(exceptions)) {}

std::size_t BatchResult::size() const noexcept {
    return exceptions_.size();
}

bool BatchResult::succeeded() const noexcept {
    return !first_exception();
}

std::exception_ptr BatchResult::exception_at(const std::size_t index) const {
    return exceptions_.at(index);
}

std::exception_ptr BatchResult::first_exception() const noexcept {
    for (const auto& exception : exceptions_) {
        if (exception) {
            return exception;
        }
    }
    return {};
}

JobSystem::JobSystem(const JobSystemConfiguration configuration)
    : implementation_(std::make_unique<Implementation>(configuration)) {}

JobSystem::~JobSystem() {
    if (!implementation_->is_owner_thread()) {
        std::_Exit(EXIT_FAILURE);
    }
}

BatchResult JobSystem::execute_batch(const std::span<const Job> batch) {
    return implementation_->execute_batch(batch);
}

void JobSystem::shutdown() {
    implementation_->shutdown();
}

std::size_t JobSystem::worker_count() const noexcept {
    return implementation_->worker_count();
}

JobSystemState JobSystem::state() const noexcept {
    return implementation_->state();
}

} // namespace game_ex::jobs
