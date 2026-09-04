/**
 * @file startup_graph_tests.cpp
 * @brief Dependency-free tests for deterministic startup graph behaviour.
 */

#include "game_ex/startup/startup_graph.hpp"

#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using game_ex::startup::LifecycleState;
using game_ex::startup::StartupGraph;
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

} // namespace

/**
 * @brief Runs all startup graph unit tests.
 * @return EXIT_SUCCESS when every lifecycle invariant holds.
 */
int main() {
    bool passed = true;
    passed &= test_registration_validation();
    passed &= test_graph_validation_before_start();
    passed &= test_deterministic_order_and_shutdown();
    passed &= test_startup_rollback();
    passed &= test_shutdown_failure_continues_cleanup();
    passed &= test_lifecycle_protection_and_raii();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
