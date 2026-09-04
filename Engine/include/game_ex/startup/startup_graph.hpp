/**
 * @file startup_graph.hpp
 * @brief Deterministic serial startup and shutdown orchestration.
 */

#pragma once

#include <cstddef>
#include <exception>
#include <memory>
#include <string>
#include <vector>

/**
 * @defgroup startup Startup lifecycle
 * @brief Owned subsystem orchestration without global service lookup.
 */

namespace game_ex::startup {

/**
 * @brief Observable lifecycle of a StartupGraph.
 * @ingroup startup
 */
enum class LifecycleState {
    /** Registrations may be added and the graph has not started. */
    configuring,

    /** A validated startup order is currently being executed. */
    starting,

    /** Every registered subsystem started successfully. */
    running,

    /** Started subsystems are currently shutting down. */
    stopping,

    /** Every started subsystem received an orderly shutdown request. */
    stopped,

    /** Startup or shutdown failed and all applicable cleanup was attempted. */
    failed
};

/**
 * @brief Lifecycle interface implemented by an ordinarily owned engine subsystem.
 *
 * Implementations receive their collaborators through normal construction. The
 * startup graph owns each implementation but deliberately provides no lookup or
 * service-locator API.
 *
 * @ingroup startup
 */
class Subsystem {
public:
    /** Enables destruction through the subsystem interface. */
    virtual ~Subsystem() = default;

    /** Subsystems have one owning registration and cannot be copied. */
    Subsystem(const Subsystem&) = delete;

    /** Subsystems cannot be copy-assigned. */
    Subsystem& operator=(const Subsystem&) = delete;

    /**
     * @brief Starts the subsystem after all declared dependencies are running.
     * @throws Any exception when startup cannot complete.
     *
     * If this function throws, StartupGraph still calls shutdown() because the
     * implementation may have acquired only some of its resources.
     */
    virtual void start() = 0;

    /**
     * @brief Releases resources acquired by start(), including partial startup.
     * @throws Any exception when cleanup encounters an error.
     *
     * Implementations must tolerate a call made after start() throws. The graph
     * calls this function at most once for each startup attempt.
     */
    virtual void shutdown() = 0;

protected:
    /** Allows construction only by concrete subsystem implementations. */
    Subsystem() = default;
};

/**
 * @brief Stable declaration and owned implementation of one startup subsystem.
 *
 * The graph copies the ID and dependency names into its own immutable entry when
 * the registration is added. Dependency names therefore do not depend on caller
 * storage or object addresses.
 *
 * @ingroup startup
 */
struct SubsystemRegistration final {
    /** Non-empty stable identifier unique within one graph. */
    std::string id;

    /** Stable identifiers that must be running before this subsystem starts. */
    std::vector<std::string> dependencies;

    /** Concrete subsystem transferred into graph ownership. */
    std::unique_ptr<Subsystem> subsystem;
};

/**
 * @brief Owns and serially executes a validated directed acyclic startup graph.
 *
 * Validation completes before the first subsystem starts. When several nodes are
 * ready simultaneously, the lexicographically smallest stable ID starts first,
 * making the order independent of registration order. Orderly shutdown and
 * startup rollback both use the exact reverse of the attempted start order.
 *
 * A graph is single-use. After successful shutdown or a lifecycle failure, create
 * a new graph and new subsystem objects for another startup attempt.
 *
 * @ingroup startup
 */
class StartupGraph final {
public:
    /** Creates an empty configurable graph. */
    StartupGraph();

    /**
     * @brief Attempts cleanup if a running graph leaves scope.
     *
     * Destruction never propagates cleanup exceptions. Call shutdown() explicitly
     * when the caller needs to observe a shutdown failure.
     */
    ~StartupGraph();

    /** A graph uniquely owns its subsystem objects and cannot be copied. */
    StartupGraph(const StartupGraph&) = delete;

    /** A graph cannot be copy-assigned. */
    StartupGraph& operator=(const StartupGraph&) = delete;

    /** Moving is disabled so subsystem collaborators may safely refer to the graph owner. */
    StartupGraph(StartupGraph&&) = delete;

    /** Move assignment is disabled for the same reason as move construction. */
    StartupGraph& operator=(StartupGraph&&) = delete;

    /**
     * @brief Adds one owned subsystem declaration.
     * @param registration Stable identity, dependencies, and implementation.
     * @throws std::logic_error if the graph is no longer configurable.
     * @throws std::invalid_argument for a null implementation, an empty or
     *         duplicate ID, or an empty or duplicate dependency ID.
     */
    void add(SubsystemRegistration registration);

    /**
     * @brief Validates and starts every subsystem in deterministic dependency order.
     * @throws std::logic_error for lifecycle misuse, a missing dependency, or a cycle.
     * @throws Any exception propagated by Subsystem::start().
     *
     * Validation failures leave the graph configurable because no start function
     * has run. A subsystem failure moves the graph to LifecycleState::failed after
     * attempting rollback of the failing subsystem and all earlier subsystems.
     */
    void start();

    /**
     * @brief Shuts down every started subsystem in reverse startup order.
     * @throws std::logic_error unless the graph is running.
     * @throws Any first exception raised by a subsystem shutdown, after cleanup
     *         has still been attempted for every remaining subsystem.
     */
    void shutdown();

    /**
     * @brief Returns the graph's current lifecycle without changing it.
     * @return Current lifecycle state.
     */
    [[nodiscard]] LifecycleState state() const noexcept;

private:
    /** Immutable stored declaration plus its owned subsystem implementation. */
    struct Entry;

    /**
     * @brief Validates dependency references and computes deterministic order.
     * @return Indices into entries_ in serial startup order.
     * @throws std::logic_error when a dependency is missing or a cycle exists.
     */
    [[nodiscard]] std::vector<std::size_t> validated_start_order() const;

    /**
     * @brief Attempts reverse-order shutdown for every recorded startup attempt.
     * @return First shutdown exception, or an empty pointer when all calls succeeded.
     */
    [[nodiscard]] std::exception_ptr shutdown_started() noexcept;

    /** Stable registrations in ownership order. */
    std::vector<std::unique_ptr<Entry>> entries_;

    /** Entry indices, including a currently failing start, in attempted order. */
    std::vector<std::size_t> started_order_;

    /** Current protection state for the single-use lifecycle. */
    LifecycleState state_{LifecycleState::configuring};
};

} // namespace game_ex::startup
