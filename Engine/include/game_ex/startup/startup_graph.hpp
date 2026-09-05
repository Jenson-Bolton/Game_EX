/**
 * @file startup_graph.hpp
 * @brief Deterministic serial and controlled-parallel startup orchestration.
 */

#pragma once

#include "game_ex/jobs/job_system.hpp"

#include <atomic>
#include <cstddef>
#include <exception>
#include <memory>
#include <string>
#include <thread>
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
 * @brief Permitted execution location for a subsystem's start operation.
 * @ingroup startup
 */
enum class StartupAffinity {
    /** Startup must execute on the graph's creating application thread. */
    main_thread,

    /** Parallel startup may use a worker; serial startup still uses the application thread. */
    worker_eligible
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

    /** Startup execution constraint; main-thread execution is the safe default. */
    StartupAffinity affinity{StartupAffinity::main_thread};
};

/**
 * @brief Owns and executes a validated directed acyclic startup graph.
 *
 * Validation completes before the first subsystem starts. When several nodes are
 * ready simultaneously, the lexicographically smallest stable ID starts first,
 * making logical admission independent of registration order. The serial path
 * invokes every node on the application thread. The controlled-parallel path
 * admits bounded worker batches and commits their settled results in lexical
 * order. Orderly shutdown and rollback both reverse logical admission order.
 *
 * A graph is single-use. After successful shutdown or a lifecycle failure, create
 * a new graph and new subsystem objects for another startup attempt.
 * The graph must be destroyed on its creating application thread. Violating this
 * affinity ends the process immediately before subsystem cleanup can run on the
 * wrong thread.
 *
 * @ingroup startup
 */
class StartupGraph final {
public:
    /** Creates an empty configurable graph owned by the calling application thread. */
    StartupGraph();

    /**
     * @brief Attempts cleanup if a running graph leaves scope.
     *
     * Destruction never propagates cleanup exceptions. Call shutdown() explicitly
     * when the caller needs to observe a shutdown failure. Destruction must occur
     * on the creating application thread; a violation calls
     * std::_Exit(EXIT_FAILURE) before any subsystem cleanup is attempted.
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
     * @throws std::logic_error outside the creating thread or if the graph is
     *         no longer configurable.
     * @throws std::invalid_argument for a null implementation, an empty or
     *         duplicate ID, or an empty or duplicate dependency ID.
     */
    void add(SubsystemRegistration registration);

    /**
     * @brief Validates and starts every subsystem in deterministic dependency order.
     * @throws std::logic_error for wrong-thread use, lifecycle misuse, a missing
     *         dependency, or a cycle.
     * @throws Any exception propagated by Subsystem::start().
     *
     * Validation failures leave the graph configurable because no start function
     * has run. A subsystem failure moves the graph to LifecycleState::failed after
     * attempting rollback of the failing subsystem and all earlier subsystems.
     */
    void start();

    /**
     * @brief Starts main-affine nodes locally and bounded eligible batches on workers.
     * @param job_system Ordinarily owned pool created on this graph's application thread.
     * @throws std::logic_error for lifecycle misuse, wrong-thread use, a missing
     *         dependency, a cycle, or an unavailable job system.
     * @throws Any deterministic first exception raised by Subsystem::start().
     *
     * Physical worker completion may vary. Batch admission, result commit, chosen
     * failure, and rollback remain deterministic by stable lexical ID. A failure
     * stops further admission after all callbacks in its accepted batch settle.
     */
    void start(jobs::JobSystem& job_system);

    /**
     * @brief Shuts down every started subsystem in reverse startup order.
     * @throws std::logic_error outside the creating thread or unless the graph is running.
     * @throws Any first exception raised by a subsystem shutdown, after cleanup
     *         has still been attempted for every remaining subsystem.
     */
    void shutdown();

    /**
     * @brief Returns the graph's current lifecycle without changing it.
     * @return Current lifecycle state.
     *
     * This observer is atomic and may be called from another thread. All graph
     * mutation and subsystem access remain confined to the creating thread or
     * to worker callbacks admitted by start(JobSystem&).
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

    /**
     * @brief Rejects graph control outside its creating application thread.
     * @param operation Human-readable operation used in diagnostics.
     * @throws std::logic_error when the current thread is not the creator.
     */
    void require_owner_thread(const char* operation) const;

    /** Stable registrations in ownership order. */
    std::vector<std::unique_ptr<Entry>> entries_;

    /** Entry indices, including a currently failing start, in attempted order. */
    std::vector<std::size_t> started_order_;

    /** Atomically observable protection state for the single-use lifecycle. */
    std::atomic<LifecycleState> state_{LifecycleState::configuring};

    /** Stable application thread required by registration and lifecycle control. */
    const std::thread::id owner_thread_;
};

} // namespace game_ex::startup
