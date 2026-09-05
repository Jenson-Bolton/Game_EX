/**
 * @file job_system.hpp
 * @brief Bounded fixed-worker execution for controlled engine parallelism.
 */

#pragma once

#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <span>
#include <vector>

/**
 * @defgroup jobs Job system
 * @brief Ordinarily owned, bounded worker execution primitives.
 */

namespace game_ex::jobs {

/** Largest explicitly supported worker pool, preventing accidental thread storms. */
inline constexpr std::size_t maximum_worker_count = 32;

/** Largest automatically selected worker pool for lightweight engine startup. */
inline constexpr std::size_t maximum_recommended_worker_count = 8;

/**
 * @brief Selects a conservative worker count from reported logical concurrency.
 * @return One to eight workers, leaving one reported logical processor for the
 *         application thread when the platform reports more than one.
 * @ingroup jobs
 */
[[nodiscard]] std::size_t recommended_worker_count() noexcept;

/**
 * @brief Fixed worker-pool construction settings.
 * @ingroup jobs
 */
struct JobSystemConfiguration final {
    /** Number of persistent worker threads to create. */
    std::size_t worker_count{recommended_worker_count()};
};

/**
 * @brief Current lifecycle of a JobSystem.
 * @ingroup jobs
 */
enum class JobSystemState {
    /** The pool accepts controlled batches. */
    running,

    /** Worker termination has been requested and joining is in progress. */
    stopping,

    /** Every worker has been joined and no more work is accepted. */
    stopped
};

/** Callable unit accepted by a controlled worker batch. */
using Job = std::function<void()>;

/**
 * @brief Per-position outcomes from a completed controlled batch.
 *
 * Results retain the caller's input order rather than physical completion order.
 * Every accepted job has returned or thrown before this object is produced.
 *
 * @ingroup jobs
 */
class BatchResult final {
public:
    /**
     * @brief Returns the number of job outcomes.
     * @return Number of positions in the completed batch.
     */
    [[nodiscard]] std::size_t size() const noexcept;

    /**
     * @brief Reports whether every job returned normally.
     * @return True when no position contains an exception.
     */
    [[nodiscard]] bool succeeded() const noexcept;

    /**
     * @brief Returns the captured exception for one input position.
     * @param index Zero-based input position.
     * @return Empty pointer for success, otherwise the job's exception.
     * @throws std::out_of_range if index is outside this result.
     */
    [[nodiscard]] std::exception_ptr exception_at(std::size_t index) const;

    /**
     * @brief Returns the first failure in deterministic input order.
     * @return First captured exception, or an empty pointer when all jobs succeeded.
     */
    [[nodiscard]] std::exception_ptr first_exception() const noexcept;

private:
    /** JobSystem creates results after a whole accepted batch has settled. */
    friend class JobSystem;

    /**
     * @brief Adopts a complete ordered outcome vector.
     * @param exceptions One captured outcome for every input job.
     */
    explicit BatchResult(std::vector<std::exception_ptr> exceptions) noexcept;

    /** Captured outcomes in caller-supplied input order. */
    std::vector<std::exception_ptr> exceptions_;
};

/**
 * @brief Owns a bounded set of persistent worker threads.
 *
 * The creating thread is the control thread. It alone may execute batches or
 * request explicit shutdown. A batch contains at most one job per worker, has no
 * unbounded queue, and is a synchronization boundary: every job settles before
 * execute_batch() returns. Calls from workers, nested calls, and calls from other
 * threads are rejected.
 *
 * The pool must also be destroyed on its creating thread. Violating that
 * invariant ends the process immediately before worker joining begins,
 * preventing a foreign-thread or worker-thread destructor from attempting
 * unsafe joins or unwinding through live workers.
 *
 * @ingroup jobs
 */
class JobSystem final {
public:
    /**
     * @brief Creates the configured fixed worker pool.
     * @param configuration Valid bounded worker count.
     * @throws std::invalid_argument if worker_count is zero or greater than
     *         maximum_worker_count.
     * @throws std::system_error if a worker thread cannot be created.
     *
     * If construction of any worker fails, every worker already created is
     * stopped and joined before the exception leaves the constructor.
     */
    explicit JobSystem(JobSystemConfiguration configuration = {});

    /**
     * @brief Stops and joins workers without propagating exceptions.
     *
     * Destruction must occur on the creating thread. A violation calls
     * std::_Exit(EXIT_FAILURE) before the implementation or worker handles are
     * destroyed.
     */
    ~JobSystem();

    /** Worker pools have unique ownership and cannot be copied. */
    JobSystem(const JobSystem&) = delete;

    /** Worker pools cannot be copy-assigned. */
    JobSystem& operator=(const JobSystem&) = delete;

    /** Moving is disabled because control-thread and worker identities are stable. */
    JobSystem(JobSystem&&) = delete;

    /** Move assignment is disabled for the same identity reason. */
    JobSystem& operator=(JobSystem&&) = delete;

    /**
     * @brief Executes one bounded batch and waits for every outcome.
     * @param batch Non-null jobs in deterministic logical admission order.
     * @return Ordered outcome for every job after the complete batch settles.
     * @throws std::logic_error when called outside the creating thread, during
     *         another batch, or after shutdown begins.
     * @throws std::invalid_argument for a null job or a batch larger than the pool.
     * @throws std::bad_alloc if batch preparation fails before publication.
     *
     * Job exceptions are captured in BatchResult and never escape this function.
     * Preparation has a strong guarantee: if it throws, no submitted job ran.
     */
    [[nodiscard]] BatchResult execute_batch(std::span<const Job> batch);

    /**
     * @brief Stops admission and joins every worker.
     * @throws std::logic_error when called outside the creating thread, while a
     *         batch is active, or after shutdown has already begun.
     */
    void shutdown();

    /**
     * @brief Returns the fixed number of workers.
     * @return Worker count selected at construction.
     */
    [[nodiscard]] std::size_t worker_count() const noexcept;

    /**
     * @brief Returns the pool lifecycle.
     * @return Current synchronized job-system state.
     */
    [[nodiscard]] JobSystemState state() const noexcept;

private:
    /** Hidden synchronization and worker implementation. */
    class Implementation;

    /** Stable implementation address observed by persistent worker threads. */
    std::unique_ptr<Implementation> implementation_;
};

} // namespace game_ex::jobs
