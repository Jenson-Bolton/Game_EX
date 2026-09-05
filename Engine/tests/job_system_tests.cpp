/**
 * @file job_system_tests.cpp
 * @brief Dependency-free tests for bounded fixed-worker execution.
 */

#include "game_ex/jobs/job_system.hpp"

#include <array>
#include <atomic>
#include <barrier>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <latch>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using game_ex::jobs::BatchResult;
using game_ex::jobs::Job;
using game_ex::jobs::JobSystem;
using game_ex::jobs::JobSystemState;

/**
 * @brief Reports one failed assertion without aborting the test executable.
 * @param condition Assertion result.
 * @param description Human-readable invariant.
 * @return True when the assertion passed.
 */
bool check(const bool condition, const std::string& description) {
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
    }
    return condition;
}

/**
 * @brief Tests whether invoking a callable raises a selected exception type.
 * @tparam Exception Expected exception base or exact type.
 * @tparam Callable Nullary callable type.
 * @param callable Operation under test.
 * @return True only when the selected exception type was caught.
 */
template <typename Exception, typename Callable>
bool throws_exception(Callable&& callable) {
    try {
        std::invoke(std::forward<Callable>(callable));
    } catch (const Exception&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

/**
 * @brief Extracts and compares a captured standard exception message.
 * @param exception Captured job exception.
 * @param expected Expected runtime-error message.
 * @return True when rethrowing produces the expected message.
 */
bool has_message(const std::exception_ptr exception, const std::string& expected) {
    if (!exception) {
        return false;
    }

    try {
        std::rethrow_exception(exception);
    } catch (const std::runtime_error& error) {
        return error.what() == expected;
    } catch (...) {
        return false;
    }
}

/**
 * @brief Raises an atomic peak value to include a new observation.
 * @param peak Shared maximum.
 * @param observation Candidate maximum.
 */
void observe_peak(std::atomic<int>& peak, const int observation) noexcept {
    int previous = peak.load();
    while (previous < observation
           && !peak.compare_exchange_weak(previous, observation)) {
    }
}

/**
 * @brief Verifies worker limits and the conservative automatic recommendation.
 * @return True when construction bounds are enforced.
 */
bool test_configuration_bounds() {
    bool passed = true;
    passed &= check(
        throws_exception<std::invalid_argument>([] { JobSystem jobs({0}); }),
        "zero workers are rejected");
    passed &= check(
        throws_exception<std::invalid_argument>([] {
            JobSystem jobs({game_ex::jobs::maximum_worker_count + 1});
        }),
        "worker counts above the hard bound are rejected");

    const std::size_t recommended = game_ex::jobs::recommended_worker_count();
    passed &= check(recommended >= 1, "automatic worker count has a positive lower bound");
    passed &= check(
        recommended <= game_ex::jobs::maximum_recommended_worker_count,
        "automatic worker count respects the conservative upper bound");
    return passed;
}

/**
 * @brief Verifies bounded work overlaps and never executes on the control thread.
 * @return True when two workers synchronize concurrently and exactly once.
 */
bool test_parallel_execution_and_bound() {
    JobSystem jobs({2});
    const std::thread::id owner = std::this_thread::get_id();
    std::array<std::thread::id, 2> worker_ids{};
    std::array<std::atomic<int>, 2> invocation_counts{};
    std::atomic<int> active{0};
    std::atomic<int> peak{0};
    std::barrier rendezvous(2);

    std::vector<Job> batch;
    batch.reserve(2);
    for (std::size_t index = 0; index < worker_ids.size(); ++index) {
        batch.emplace_back([&, index] {
            ++invocation_counts[index];
            worker_ids[index] = std::this_thread::get_id();
            const int now_active = active.fetch_add(1) + 1;
            observe_peak(peak, now_active);
            rendezvous.arrive_and_wait();
            --active;
        });
    }

    const BatchResult result = jobs.execute_batch(batch);
    bool passed = check(result.succeeded(), "successful batch reports no exceptions");
    passed &= check(result.size() == 2, "result retains one slot per input job");
    passed &= check(peak.load() == 2, "two-worker batch overlaps exactly two jobs");
    passed &= check(
        invocation_counts[0].load() == 1 && invocation_counts[1].load() == 1,
        "each accepted job executes exactly once");
    passed &= check(
        worker_ids[0] != owner && worker_ids[1] != owner,
        "jobs execute outside the control thread");
    passed &= check(worker_ids[0] != worker_ids[1], "overlapping jobs use distinct workers");

    std::vector<Job> oversized(3, [] {});
    passed &= check(
        throws_exception<std::invalid_argument>([&jobs, &oversized] {
            static_cast<void>(jobs.execute_batch(oversized));
        }),
        "batch cannot exceed the fixed worker count");

    Job empty_job;
    const std::array<Job, 1> invalid_batch{empty_job};
    passed &= check(
        throws_exception<std::invalid_argument>([&jobs, &invalid_batch] {
            static_cast<void>(jobs.execute_batch(invalid_batch));
        }),
        "empty callable is rejected before publication");
    return passed;
}

/**
 * @brief Verifies exception capture follows input order, not completion timing.
 * @return True when all failures settle and remain position-addressable.
 */
bool test_deterministic_exception_results() {
    JobSystem jobs({2});
    std::atomic<int> failure_sequence{0};
    std::array<int, 2> failure_order{-1, -1};
    std::barrier both_started(2);
    std::latch later_failure_released(1);
    std::vector<Job> batch;
    batch.emplace_back([&] {
        both_started.arrive_and_wait();
        later_failure_released.wait();
        failure_order[0] = failure_sequence.fetch_add(1);
        throw std::runtime_error("alpha failure");
    });
    batch.emplace_back([&] {
        both_started.arrive_and_wait();
        failure_order[1] = failure_sequence.fetch_add(1);
        later_failure_released.count_down();
        throw std::runtime_error("bravo failure");
    });

    const BatchResult result = jobs.execute_batch(batch);
    bool passed = check(!result.succeeded(), "exceptional batch reports failure");
    passed &= check(
        failure_order[1] == 0 && failure_order[0] == 1,
        "controlled failure points are the reverse of logical result order");
    passed &= check(
        has_message(result.exception_at(0), "alpha failure"),
        "first input retains its later failure");
    passed &= check(
        has_message(result.exception_at(1), "bravo failure"),
        "second input retains its earlier failure");
    passed &= check(
        has_message(result.first_exception(), "alpha failure"),
        "first exception is selected by deterministic input order");
    passed &= check(
        throws_exception<std::out_of_range>([&result] {
            static_cast<void>(result.exception_at(2));
        }),
        "out-of-range result access is rejected");
    return passed;
}

/**
 * @brief Verifies creator-thread, nested-call, and terminal lifecycle protection.
 * @return True when invalid control operations are rejected without deadlock.
 */
bool test_control_thread_and_shutdown_protection() {
    bool passed = true;
    JobSystem jobs({1});
    const std::vector<Job> empty_batch;
    std::atomic<bool> foreign_execute_rejected{false};
    std::latch active_job_started(1);
    std::latch foreign_execute_finished(1);
    std::thread foreign([&] {
        active_job_started.wait();
        try {
            static_cast<void>(jobs.execute_batch(empty_batch));
        } catch (const std::logic_error&) {
            foreign_execute_rejected = true;
        }
        foreign_execute_finished.count_down();
    });

    const std::array<Job, 1> active_batch{Job{[&] {
        active_job_started.count_down();
        foreign_execute_finished.wait();
    }}};
    const BatchResult active_result = jobs.execute_batch(active_batch);
    foreign.join();
    passed &= check(active_result.succeeded(), "active control batch completes normally");
    passed &= check(
        foreign_execute_rejected.load(),
        "concurrent foreign-thread batch is rejected while work is active");

    std::atomic<bool> nested_execute_rejected{false};
    const std::array<Job, 1> nested_batch{Job{[&] {
        try {
            static_cast<void>(jobs.execute_batch(empty_batch));
        } catch (const std::logic_error&) {
            nested_execute_rejected = true;
        }
    }}};
    const BatchResult nested_result = jobs.execute_batch(nested_batch);
    passed &= check(nested_result.succeeded(), "worker can handle a caught nested rejection");
    passed &= check(nested_execute_rejected.load(), "nested worker batch is rejected");

    passed &= check(
        jobs.state() == JobSystemState::running,
        "pool reports running before explicit shutdown");
    jobs.shutdown();
    passed &= check(jobs.state() == JobSystemState::stopped, "pool reports stopped after join");
    passed &= check(
        throws_exception<std::logic_error>([&jobs, &empty_batch] {
            static_cast<void>(jobs.execute_batch(empty_batch));
        }),
        "batch after shutdown is rejected");
    passed &= check(
        throws_exception<std::logic_error>([&jobs] { jobs.shutdown(); }),
        "second shutdown is rejected");
    return passed;
}

} // namespace

/**
 * @brief Runs all bounded job-system unit tests.
 * @param argument_count Number of process command-line arguments.
 * @param arguments Process command-line arguments, including death-test mode.
 * @return EXIT_SUCCESS when every worker and lifecycle invariant holds.
 */
int main(const int argument_count, char* arguments[]) {
    if (argument_count == 2
        && std::string(arguments[1]) == "--death-destroy-outside-creator") {
        auto jobs = std::make_unique<JobSystem>(game_ex::jobs::JobSystemConfiguration{1});
        std::thread foreign_destroyer([owned_jobs = std::move(jobs)]() mutable {
            owned_jobs.reset();
        });
        foreign_destroyer.join();
        return EXIT_SUCCESS;
    }
    if (argument_count != 1) {
        return EXIT_FAILURE;
    }

    bool passed = true;
    passed &= test_configuration_bounds();
    passed &= test_parallel_execution_and_bound();
    passed &= test_deterministic_exception_results();
    passed &= test_control_thread_and_shutdown_protection();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
