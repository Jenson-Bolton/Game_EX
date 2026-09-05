/**
 * @file startup_graph_tests.cpp
 * @brief Dependency-free tests for deterministic startup graph behaviour.
 */

#include "game_ex/startup/startup_graph.hpp"

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

using game_ex::startup::LifecycleState;
using game_ex::startup::StartupGraph;
using game_ex::startup::StartupAffinity;
using game_ex::startup::Subsystem;
using game_ex::startup::SubsystemRegistration;

/**
 * @brief Test subsystem that records calls and can fail either lifecycle phase.
 */
class RecordingSubsystem final : public Subsystem {
public:
    /**
     * @brief Creates a controllable recording subsystem.
     * @param name Label written to the shared event list.
     * @param events Shared ordered lifecycle evidence.
     * @param fail_start Whether start() throws after recording its call.
     * @param fail_shutdown Whether shutdown() throws after recording its call.
     */
    RecordingSubsystem(
        std::string name,
        std::vector<std::string>& events,
        const bool fail_start = false,
        const bool fail_shutdown = false)
        : name_(std::move(name)),
          events_(events),
          fail_start_(fail_start),
          fail_shutdown_(fail_shutdown) {}

    /** @copydoc Subsystem::start */
    void start() override {
        events_.push_back(name_ + ".start");
        if (fail_start_) {
            throw std::runtime_error(name_ + " start failed");
        }
    }

    /** @copydoc Subsystem::shutdown */
    void shutdown() override {
        events_.push_back(name_ + ".shutdown");
        if (fail_shutdown_) {
            throw std::runtime_error(name_ + " shutdown failed");
        }
    }

private:
    /** Stable label used only for readable test evidence. */
    std::string name_;

    /** Non-owning event sink whose lifetime encloses the graph. */
    std::vector<std::string>& events_;

    /** Injected startup failure flag. */
    bool fail_start_;

    /** Injected shutdown failure flag. */
    bool fail_shutdown_;
};

/**
 * @brief Test subsystem whose lifecycle is supplied by callbacks.
 */
class CallbackSubsystem final : public Subsystem {
public:
    /**
     * @brief Stores test lifecycle callbacks.
     * @param start_callback Operation used for start().
     * @param shutdown_callback Operation used for shutdown().
     */
    CallbackSubsystem(
        std::function<void()> start_callback,
        std::function<void()> shutdown_callback)
        : start_callback_(std::move(start_callback)),
          shutdown_callback_(std::move(shutdown_callback)) {}

    /** @copydoc Subsystem::start */
    void start() override {
        start_callback_();
    }

    /** @copydoc Subsystem::shutdown */
    void shutdown() override {
        shutdown_callback_();
    }

private:
    /** Injected startup behavior. */
    std::function<void()> start_callback_;

    /** Injected shutdown behavior. */
    std::function<void()> shutdown_callback_;
};

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
 * @brief Creates one recording registration with concise test syntax.
 * @param id Stable graph identifier and default event label.
 * @param dependencies Required subsystem IDs.
 * @param events Shared ordered event sink.
 * @param fail_start Whether startup should fail.
 * @param fail_shutdown Whether shutdown should fail.
 * @return Owned subsystem registration.
 */
SubsystemRegistration registration(
    std::string id,
    std::vector<std::string> dependencies,
    std::vector<std::string>& events,
    const bool fail_start = false,
    const bool fail_shutdown = false) {
    const std::string label = id;
    return {
        std::move(id),
        std::move(dependencies),
        std::make_unique<RecordingSubsystem>(
            label,
            events,
            fail_start,
            fail_shutdown)};
}

/**
 * @brief Creates an affinity-aware callback registration for parallel tests.
 * @param id Stable graph identifier.
 * @param dependencies Required subsystem IDs.
 * @param affinity Startup execution constraint.
 * @param start_callback Operation used for start().
 * @param shutdown_callback Operation used for shutdown().
 * @return Owned callback subsystem registration.
 */
SubsystemRegistration callback_registration(
    std::string id,
    std::vector<std::string> dependencies,
    const StartupAffinity affinity,
    std::function<void()> start_callback,
    std::function<void()> shutdown_callback) {
    return {
        std::move(id),
        std::move(dependencies),
        std::make_unique<CallbackSubsystem>(
            std::move(start_callback), std::move(shutdown_callback)),
        affinity};
}

/**
 * @brief Verifies invalid registrations are rejected immediately.
 * @return True when all registration checks pass.
 */
bool test_registration_validation() {
    bool passed = true;
    std::vector<std::string> events;
    StartupGraph graph;

    passed &= check(
        throws_exception<std::invalid_argument>([&graph] {
            graph.add({"null", {}, nullptr});
        }),
        "null subsystem is rejected");

    passed &= check(
        throws_exception<std::invalid_argument>([&graph, &events] {
            graph.add(registration("", {}, events));
        }),
        "empty subsystem ID is rejected");

    passed &= check(
        throws_exception<std::invalid_argument>([&graph, &events] {
            graph.add(registration("empty-dependency", {""}, events));
        }),
        "empty dependency ID is rejected");

    graph.add(registration("unique", {}, events));
    passed &= check(
        throws_exception<std::invalid_argument>([&graph, &events] {
            graph.add(registration("unique", {}, events));
        }),
        "duplicate subsystem ID is rejected");

    passed &= check(
        throws_exception<std::invalid_argument>([&graph, &events] {
            graph.add(registration("duplicate-dependency", {"unique", "unique"}, events));
        }),
        "duplicate dependency ID is rejected");

    return passed;
}

/**
 * @brief Verifies missing dependencies and cycles fail before any start call.
 * @return True when whole-graph validation is atomic.
 */
bool test_graph_validation_before_start() {
    bool passed = true;

    {
        std::vector<std::string> events;
        StartupGraph graph;
        graph.add(registration("consumer", {"missing"}, events));

        passed &= check(
            throws_exception<std::logic_error>([&graph] { graph.start(); }),
            "missing dependency is rejected");
        passed &= check(events.empty(), "missing dependency starts no subsystem");
        passed &= check(
            graph.state() == LifecycleState::configuring,
            "validation failure leaves graph configurable");

        graph.add(registration("missing", {}, events));
        graph.start();
        graph.shutdown();
    }

    {
        std::vector<std::string> events;
        StartupGraph graph;
        graph.add(registration("alpha", {"bravo"}, events));
        graph.add(registration("bravo", {"alpha"}, events));
        graph.add(registration("charlie", {"bravo"}, events));

        bool reported_unresolved_nodes = false;
        try {
            graph.start();
        } catch (const std::logic_error& error) {
            const std::string message = error.what();
            reported_unresolved_nodes = message.find("cycle prevents resolution of")
                    != std::string::npos
                && message.find("alpha") != std::string::npos
                && message.find("bravo") != std::string::npos
                && message.find("charlie") != std::string::npos;
        }
        passed &= check(reported_unresolved_nodes, "cycle and blocked dependent are reported honestly");
        passed &= check(events.empty(), "cycle starts no subsystem");
    }

    return passed;
}

/**
 * @brief Verifies lexical ready-node ordering and exact reverse shutdown.
 * @return True when deterministic lifecycle order matches the contract.
 */
bool test_deterministic_order_and_shutdown() {
    std::vector<std::string> events;
    StartupGraph graph;

    graph.add(registration("telemetry", {}, events));
    graph.add(registration("gameplay", {"data", "renderer"}, events));
    graph.add(registration("renderer", {"platform"}, events));
    graph.add(registration("data", {"platform"}, events));
    graph.add(registration("platform", {}, events));

    graph.start();
    bool passed = check(
        events
            == std::vector<std::string>{
                "platform.start",
                "data.start",
                "renderer.start",
                "gameplay.start",
                "telemetry.start"},
        "ready subsystems start by lexical stable ID");
    passed &= check(graph.state() == LifecycleState::running, "graph reaches running state");

    graph.shutdown();
    passed &= check(
        events
            == std::vector<std::string>{
                "platform.start",
                "data.start",
                "renderer.start",
                "gameplay.start",
                "telemetry.start",
                "telemetry.shutdown",
                "gameplay.shutdown",
                "renderer.shutdown",
                "data.shutdown",
                "platform.shutdown"},
        "shutdown exactly reverses startup order");
    passed &= check(graph.state() == LifecycleState::stopped, "graph reaches stopped state");

    return passed;
}

/**
 * @brief Verifies rollback includes a partially started failing subsystem.
 * @return True when rollback is complete and preserves the startup exception.
 */
bool test_startup_rollback() {
    std::vector<std::string> events;
    StartupGraph graph;
    graph.add(registration("alpha", {}, events));
    graph.add(registration("bravo", {"alpha"}, events, true, true));
    graph.add(registration("charlie", {"bravo"}, events));

    bool caught_expected_failure = false;
    try {
        graph.start();
    } catch (const std::runtime_error& error) {
        caught_expected_failure = std::string(error.what()) == "bravo start failed";
    }

    bool passed = check(caught_expected_failure, "startup preserves the original start failure");
    passed &= check(
        events
            == std::vector<std::string>{
                "alpha.start",
                "bravo.start",
                "bravo.shutdown",
                "alpha.shutdown"},
        "rollback cleans failing partial subsystem then earlier subsystem");
    passed &= check(graph.state() == LifecycleState::failed, "startup failure is terminal");
    return passed;
}

/**
 * @brief Verifies shutdown continues after an individual subsystem throws.
 * @return True when all cleanup is attempted and the first failure is reported.
 */
bool test_shutdown_failure_continues_cleanup() {
    std::vector<std::string> events;
    StartupGraph graph;
    graph.add(registration("alpha", {}, events, false, true));
    graph.add(registration("bravo", {"alpha"}, events, false, true));
    graph.start();

    bool caught_first_failure = false;
    try {
        graph.shutdown();
    } catch (const std::runtime_error& error) {
        caught_first_failure = std::string(error.what()) == "bravo shutdown failed";
    }

    bool passed = check(caught_first_failure, "shutdown reports first reverse-order failure");
    passed &= check(
        events
            == std::vector<std::string>{
                "alpha.start",
                "bravo.start",
                "bravo.shutdown",
                "alpha.shutdown"},
        "shutdown failure does not skip remaining cleanup");
    passed &= check(graph.state() == LifecycleState::failed, "shutdown failure is terminal");
    return passed;
}

/**
 * @brief Verifies invalid lifecycle transitions and destructor cleanup.
 * @return True when single-use state protections hold.
 */
bool test_lifecycle_protection_and_raii() {
    bool passed = true;
    std::vector<std::string> events;

    {
        StartupGraph graph;
        passed &= check(
            throws_exception<std::logic_error>([&graph] { graph.shutdown(); }),
            "shutdown before start is rejected");

        graph.add(registration("owned", {}, events));
        graph.start();
        passed &= check(
            throws_exception<std::logic_error>([&graph] { graph.start(); }),
            "second start is rejected");
        passed &= check(
            throws_exception<std::logic_error>([&graph, &events] {
                graph.add(registration("late", {}, events));
            }),
            "registration after start is rejected");
    }

    passed &= check(
        events == std::vector<std::string>{"owned.start", "owned.shutdown"},
        "destructor shuts down a still-running graph");

    {
        StartupGraph graph;
        graph.add(registration("single-use", {}, events));
        graph.start();
        graph.shutdown();
        passed &= check(
            throws_exception<std::logic_error>([&graph] { graph.shutdown(); }),
            "second shutdown is rejected");
        passed &= check(
            throws_exception<std::logic_error>([&graph] { graph.start(); }),
            "stopped graph cannot restart");
    }

    return passed;
}

/**
 * @brief Verifies worker overlap, dependency barriers, affinity, and rollback order.
 * @return True when successful controlled-parallel startup follows its logical order.
 */
bool test_parallel_affinity_and_dependencies() {
    game_ex::jobs::JobSystem jobs({2});
    const std::thread::id owner = std::this_thread::get_id();
    std::array<std::thread::id, 2> worker_threads{};
    std::array<std::atomic<bool>, 2> worker_complete{};
    std::barrier worker_rendezvous(2);
    bool dependent_saw_complete_workers = false;
    bool dependent_ran_on_owner = false;
    bool shutdowns_ran_on_owner = true;
    std::vector<std::string> shutdown_order;
    StartupGraph graph;

    graph.add(callback_registration(
        "charlie.main",
        {"alpha.worker", "bravo.worker"},
        StartupAffinity::main_thread,
        [&] {
            dependent_saw_complete_workers = worker_complete[0].load()
                && worker_complete[1].load();
            dependent_ran_on_owner = std::this_thread::get_id() == owner;
        },
        [&] {
            shutdowns_ran_on_owner &= std::this_thread::get_id() == owner;
            shutdown_order.emplace_back("charlie.main");
        }));
    graph.add(callback_registration(
        "bravo.worker",
        {},
        StartupAffinity::worker_eligible,
        [&] {
            worker_threads[1] = std::this_thread::get_id();
            worker_rendezvous.arrive_and_wait();
            worker_complete[1] = true;
        },
        [&] {
            shutdowns_ran_on_owner &= std::this_thread::get_id() == owner;
            shutdown_order.emplace_back("bravo.worker");
        }));
    graph.add(callback_registration(
        "alpha.worker",
        {},
        StartupAffinity::worker_eligible,
        [&] {
            worker_threads[0] = std::this_thread::get_id();
            worker_rendezvous.arrive_and_wait();
            worker_complete[0] = true;
        },
        [&] {
            shutdowns_ran_on_owner &= std::this_thread::get_id() == owner;
            shutdown_order.emplace_back("alpha.worker");
        }));

    graph.start(jobs);
    bool passed = check(
        worker_threads[0] != owner && worker_threads[1] != owner,
        "worker-eligible startup runs outside the application thread");
    passed &= check(
        worker_threads[0] != worker_threads[1],
        "independent worker-eligible subsystems overlap on two workers");
    passed &= check(
        dependent_saw_complete_workers,
        "dependent starts only after the complete prerequisite batch commits");
    passed &= check(dependent_ran_on_owner, "main-affine dependent runs on graph owner");

    graph.shutdown();
    passed &= check(shutdowns_ran_on_owner, "all shutdown callbacks run on graph owner");
    passed &= check(
        shutdown_order
            == std::vector<std::string>{
                "charlie.main", "bravo.worker", "alpha.worker"},
        "shutdown reverses deterministic logical admission order");

    std::thread::id serial_thread;
    StartupGraph serial_graph;
    serial_graph.add(callback_registration(
        "worker-eligible",
        {},
        StartupAffinity::worker_eligible,
        [&serial_thread] { serial_thread = std::this_thread::get_id(); },
        [] {}));
    serial_graph.start();
    serial_graph.shutdown();
    passed &= check(
        serial_thread == owner,
        "serial compatibility path runs worker-eligible nodes on caller");
    return passed;
}

/**
 * @brief Verifies a failed worker batch settles before deterministic rollback.
 * @return True when lexical failure selection prevents all later admission.
 */
bool test_parallel_failure_stops_admission() {
    game_ex::jobs::JobSystem jobs({2});
    const std::thread::id owner = std::this_thread::get_id();
    std::barrier failure_rendezvous(2);
    std::latch later_failure_released(1);
    std::atomic<int> attempted{0};
    std::atomic<int> failure_sequence{0};
    std::array<int, 2> failure_order{-1, -1};
    std::atomic<bool> charlie_started{false};
    bool shutdowns_ran_on_owner = true;
    std::vector<std::string> shutdown_order;
    StartupGraph graph;

    graph.add(callback_registration(
        "charlie.worker",
        {},
        StartupAffinity::worker_eligible,
        [&charlie_started] { charlie_started = true; },
        [&shutdown_order] { shutdown_order.emplace_back("charlie.worker"); }));
    graph.add(callback_registration(
        "bravo.worker",
        {},
        StartupAffinity::worker_eligible,
        [&] {
            ++attempted;
            failure_rendezvous.arrive_and_wait();
            failure_order[1] = failure_sequence.fetch_add(1);
            later_failure_released.count_down();
            throw std::runtime_error("bravo failure");
        },
        [&] {
            shutdowns_ran_on_owner &= std::this_thread::get_id() == owner;
            shutdown_order.emplace_back("bravo.worker");
        }));
    graph.add(callback_registration(
        "alpha.worker",
        {},
        StartupAffinity::worker_eligible,
        [&] {
            ++attempted;
            failure_rendezvous.arrive_and_wait();
            later_failure_released.wait();
            failure_order[0] = failure_sequence.fetch_add(1);
            throw std::runtime_error("alpha failure");
        },
        [&] {
            shutdowns_ran_on_owner &= std::this_thread::get_id() == owner;
            shutdown_order.emplace_back("alpha.worker");
        }));

    bool caught_lexical_failure = false;
    try {
        graph.start(jobs);
    } catch (const std::runtime_error& error) {
        caught_lexical_failure = std::string(error.what()) == "alpha failure";
    }

    bool passed = check(attempted.load() == 2, "every admitted failing-batch node settles");
    passed &= check(
        failure_order[1] == 0 && failure_order[0] == 1,
        "controlled failure points invert lexical admission order");
    passed &= check(caught_lexical_failure, "lexically first admitted failure is propagated");
    passed &= check(!charlie_started.load(), "failure prevents admission of the next bounded batch");
    passed &= check(shutdowns_ran_on_owner, "parallel rollback runs on graph owner");
    passed &= check(
        shutdown_order == std::vector<std::string>{"bravo.worker", "alpha.worker"},
        "parallel rollback reverses logical admission, not controlled failure order");
    passed &= check(graph.state() == LifecycleState::failed, "parallel failure is terminal");
    return passed;
}

/**
 * @brief Verifies graph control and job-system availability before any start call.
 * @return True when wrong-thread and stopped-pool attempts leave configuration intact.
 */
bool test_parallel_control_protection() {
    bool passed = true;
    std::atomic<bool> foreign_start_rejected{false};
    bool started = false;
    StartupGraph graph;
    graph.add(callback_registration(
        "main",
        {},
        StartupAffinity::main_thread,
        [&started] { started = true; },
        [] {}));

    std::thread foreign([&] {
        try {
            graph.start();
        } catch (const std::logic_error&) {
            foreign_start_rejected = true;
        }
    });
    foreign.join();
    passed &= check(foreign_start_rejected.load(), "graph start outside creator thread is rejected");
    passed &= check(!started, "wrong-thread graph start invokes no subsystem");
    passed &= check(
        graph.state() == LifecycleState::configuring,
        "wrong-thread rejection preserves configurable state");

    game_ex::jobs::JobSystem stopped_jobs({1});
    stopped_jobs.shutdown();
    passed &= check(
        throws_exception<std::logic_error>([&graph, &stopped_jobs] {
            graph.start(stopped_jobs);
        }),
        "parallel startup rejects a stopped job system before main-affine work");
    passed &= check(!started, "stopped job system invokes no subsystem");
    passed &= check(
        graph.state() == LifecycleState::configuring,
        "unavailable job system leaves graph configurable");

    graph.start();
    graph.shutdown();
    passed &= check(started, "graph remains usable after pre-start control rejection");
    return passed;
}

} // namespace

/**
 * @brief Runs all startup graph unit tests.
 * @param argument_count Number of process command-line arguments.
 * @param arguments Process command-line arguments, including death-test mode.
 * @return EXIT_SUCCESS when every lifecycle invariant holds.
 */
int main(const int argument_count, char* arguments[]) {
    if (argument_count == 2
        && std::string(arguments[1]) == "--death-destroy-outside-creator") {
        std::vector<std::string> events;
        auto graph = std::make_unique<StartupGraph>();
        graph->add(registration("main-affine", {}, events));
        graph->start();
        std::thread foreign_destroyer([owned_graph = std::move(graph)]() mutable {
            owned_graph.reset();
        });
        foreign_destroyer.join();
        return EXIT_SUCCESS;
    }
    if (argument_count != 1) {
        return EXIT_FAILURE;
    }

    bool passed = true;
    passed &= test_registration_validation();
    passed &= test_graph_validation_before_start();
    passed &= test_deterministic_order_and_shutdown();
    passed &= test_startup_rollback();
    passed &= test_shutdown_failure_continues_cleanup();
    passed &= test_lifecycle_protection_and_raii();
    passed &= test_parallel_affinity_and_dependencies();
    passed &= test_parallel_failure_stops_admission();
    passed &= test_parallel_control_protection();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
