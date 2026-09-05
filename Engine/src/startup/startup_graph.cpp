/**
 * @file startup_graph.cpp
 * @brief Implementation of deterministic serial and controlled-parallel startup.
 */

#include "game_ex/startup/startup_graph.hpp"

#include <cstdlib>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace game_ex::startup {

/**
 * @brief Immutable graph metadata and its ordinarily owned implementation.
 */
struct StartupGraph::Entry final {
    /**
     * @brief Takes ownership of a validated subsystem registration.
     * @param stable_id Unique non-empty graph identifier.
     * @param dependency_ids Required stable identifiers.
     * @param implementation Concrete owned subsystem.
     * @param startup_affinity Permitted startup execution location.
     */
    Entry(
        std::string stable_id,
        std::vector<std::string> dependency_ids,
        std::unique_ptr<Subsystem> implementation,
        const StartupAffinity startup_affinity)
        : id(std::move(stable_id)),
          dependencies(std::move(dependency_ids)),
          subsystem(std::move(implementation)),
          affinity(startup_affinity) {}

    /** Stable identifier copied from the public registration. */
    const std::string id;

    /** Stable dependency identifiers copied from the public registration. */
    const std::vector<std::string> dependencies;

    /** Concrete subsystem with ordinary unique ownership. */
    std::unique_ptr<Subsystem> subsystem;

    /** Declared location constraint for startup execution. */
    const StartupAffinity affinity;
};

StartupGraph::StartupGraph() : owner_thread_(std::this_thread::get_id()) {}

StartupGraph::~StartupGraph() {
    if (std::this_thread::get_id() != owner_thread_) {
        std::_Exit(EXIT_FAILURE);
    }

    if (state_.load() == LifecycleState::running) {
        state_.store(LifecycleState::stopping);
        const std::exception_ptr failure = shutdown_started();
        state_.store(failure ? LifecycleState::failed : LifecycleState::stopped);
    }
}

void StartupGraph::add(SubsystemRegistration registration) {
    require_owner_thread("add a startup subsystem");

    if (state_.load() != LifecycleState::configuring) {
        throw std::logic_error("Subsystems can only be added while the startup graph is configuring");
    }

    if (!registration.subsystem) {
        throw std::invalid_argument("Startup subsystem implementation cannot be null");
    }

    if (registration.id.empty()) {
        throw std::invalid_argument("Startup subsystem ID cannot be empty");
    }

    for (const auto& entry : entries_) {
        if (entry->id == registration.id) {
            throw std::invalid_argument("Duplicate startup subsystem ID: " + registration.id);
        }
    }

    std::set<std::string, std::less<>> unique_dependencies;
    for (const auto& dependency : registration.dependencies) {
        if (dependency.empty()) {
            throw std::invalid_argument(
                "Startup dependency ID cannot be empty for subsystem: " + registration.id);
        }
        if (!unique_dependencies.insert(dependency).second) {
            throw std::invalid_argument(
                "Duplicate startup dependency '" + dependency + "' for subsystem: "
                + registration.id);
        }
    }

    entries_.push_back(std::make_unique<Entry>(
        std::move(registration.id),
        std::move(registration.dependencies),
        std::move(registration.subsystem),
        registration.affinity));
}

void StartupGraph::start() {
    require_owner_thread("start the startup graph");

    if (state_.load() != LifecycleState::configuring) {
        throw std::logic_error("Startup graph can only start from the configuring state");
    }

    const std::vector<std::size_t> order = validated_start_order();
    // Reserve before any callback runs so recording a completed attempt cannot allocate.
    started_order_.reserve(entries_.size());
    state_.store(LifecycleState::starting);

    try {
        for (const std::size_t index : order) {
            started_order_.push_back(index);
            entries_[index]->subsystem->start();
        }
    } catch (...) {
        const std::exception_ptr startup_failure = std::current_exception();
        const std::exception_ptr cleanup_failure = shutdown_started();
        static_cast<void>(cleanup_failure);
        state_.store(LifecycleState::failed);
        std::rethrow_exception(startup_failure);
    }

    state_.store(LifecycleState::running);
}

void StartupGraph::start(jobs::JobSystem& job_system) {
    require_owner_thread("start the startup graph");

    if (state_.load() != LifecycleState::configuring) {
        throw std::logic_error("Startup graph can only start from the configuring state");
    }

    const std::vector<std::size_t> validation_order = validated_start_order();
    static_cast<void>(validation_order);

    const std::span<const jobs::Job> empty_batch;
    static_cast<void>(job_system.execute_batch(empty_batch));

    std::map<std::string, std::size_t, std::less<>> index_by_id;
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        index_by_id.emplace(entries_[index]->id, index);
    }

    std::vector<std::size_t> remaining_dependencies(entries_.size(), 0);
    std::vector<std::vector<std::size_t>> dependents(entries_.size());
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        remaining_dependencies[index] = entries_[index]->dependencies.size();
        for (const auto& dependency : entries_[index]->dependencies) {
            dependents[index_by_id.at(dependency)].push_back(index);
        }
    }

    std::set<std::pair<std::string, std::size_t>> ready;
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        if (remaining_dependencies[index] == 0) {
            ready.emplace(entries_[index]->id, index);
        }
    }

    // A settled worker batch is recorded only after execution. Full reservation
    // makes that size_t-only insert non-allocating and therefore non-throwing.
    started_order_.reserve(entries_.size());
    state_.store(LifecycleState::starting);

    try {
        while (!ready.empty()) {
            const std::size_t first_index = ready.begin()->second;
            if (entries_[first_index]->affinity == StartupAffinity::main_thread) {
                ready.erase(ready.begin());
                started_order_.push_back(first_index);
                entries_[first_index]->subsystem->start();

                for (const std::size_t dependent : dependents[first_index]) {
                    --remaining_dependencies[dependent];
                    if (remaining_dependencies[dependent] == 0) {
                        ready.emplace(entries_[dependent]->id, dependent);
                    }
                }
                continue;
            }

            std::vector<std::size_t> batch_indices;
            batch_indices.reserve(job_system.worker_count());
            auto candidate = ready.begin();
            while (candidate != ready.end()
                   && batch_indices.size() < job_system.worker_count()) {
                const std::size_t index = candidate->second;
                if (entries_[index]->affinity != StartupAffinity::worker_eligible) {
                    break;
                }
                batch_indices.push_back(index);
                candidate = ready.erase(candidate);
            }

            std::vector<jobs::Job> batch;
            batch.reserve(batch_indices.size());
            for (const std::size_t index : batch_indices) {
                batch.emplace_back([this, index] { entries_[index]->subsystem->start(); });
            }

            const jobs::BatchResult result = job_system.execute_batch(batch);
            started_order_.insert(
                started_order_.end(), batch_indices.begin(), batch_indices.end());

            const std::exception_ptr failure = result.first_exception();
            if (failure) {
                std::rethrow_exception(failure);
            }

            for (const std::size_t index : batch_indices) {
                for (const std::size_t dependent : dependents[index]) {
                    --remaining_dependencies[dependent];
                    if (remaining_dependencies[dependent] == 0) {
                        ready.emplace(entries_[dependent]->id, dependent);
                    }
                }
            }
        }
    } catch (...) {
        const std::exception_ptr startup_failure = std::current_exception();
        const std::exception_ptr cleanup_failure = shutdown_started();
        static_cast<void>(cleanup_failure);
        state_.store(LifecycleState::failed);
        std::rethrow_exception(startup_failure);
    }

    state_.store(LifecycleState::running);
}

void StartupGraph::shutdown() {
    require_owner_thread("shut down the startup graph");

    if (state_.load() != LifecycleState::running) {
        throw std::logic_error("Startup graph can only shut down from the running state");
    }

    state_.store(LifecycleState::stopping);
    const std::exception_ptr failure = shutdown_started();
    state_.store(failure ? LifecycleState::failed : LifecycleState::stopped);

    if (failure) {
        std::rethrow_exception(failure);
    }
}

LifecycleState StartupGraph::state() const noexcept {
    return state_.load();
}

std::vector<std::size_t> StartupGraph::validated_start_order() const {
    std::map<std::string, std::size_t, std::less<>> index_by_id;
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        index_by_id.emplace(entries_[index]->id, index);
    }

    std::vector<std::size_t> remaining_dependencies(entries_.size(), 0);
    std::vector<std::vector<std::size_t>> dependents(entries_.size());

    for (std::size_t index = 0; index < entries_.size(); ++index) {
        remaining_dependencies[index] = entries_[index]->dependencies.size();
        for (const auto& dependency : entries_[index]->dependencies) {
            const auto dependency_entry = index_by_id.find(dependency);
            if (dependency_entry == index_by_id.end()) {
                throw std::logic_error(
                    "Startup subsystem '" + entries_[index]->id
                    + "' requires missing dependency '" + dependency + "'");
            }
            dependents[dependency_entry->second].push_back(index);
        }
    }

    std::set<std::pair<std::string, std::size_t>> ready;
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        if (remaining_dependencies[index] == 0) {
            ready.emplace(entries_[index]->id, index);
        }
    }

    std::vector<std::size_t> order;
    order.reserve(entries_.size());
    while (!ready.empty()) {
        const auto ready_entry = ready.begin();
        const std::size_t index = ready_entry->second;
        ready.erase(ready_entry);
        order.push_back(index);

        for (const std::size_t dependent : dependents[index]) {
            --remaining_dependencies[dependent];
            if (remaining_dependencies[dependent] == 0) {
                ready.emplace(entries_[dependent]->id, dependent);
            }
        }
    }

    if (order.size() != entries_.size()) {
        std::ostringstream message;
        message << "Startup dependency cycle prevents resolution of:";
        for (const auto& [id, index] : index_by_id) {
            if (remaining_dependencies[index] != 0) {
                message << ' ' << id;
            }
        }
        throw std::logic_error(message.str());
    }

    return order;
}

std::exception_ptr StartupGraph::shutdown_started() noexcept {
    std::exception_ptr first_failure;

    for (auto position = started_order_.rbegin(); position != started_order_.rend(); ++position) {
        try {
            entries_[*position]->subsystem->shutdown();
        } catch (...) {
            if (!first_failure) {
                first_failure = std::current_exception();
            }
        }
    }

    started_order_.clear();
    return first_failure;
}

void StartupGraph::require_owner_thread(const char* const operation) const {
    if (std::this_thread::get_id() != owner_thread_) {
        throw std::logic_error(std::string("Only the creating thread may ") + operation);
    }
}

} // namespace game_ex::startup
