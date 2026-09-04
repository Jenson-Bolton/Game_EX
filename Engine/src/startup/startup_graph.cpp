/**
 * @file startup_graph.cpp
 * @brief Implementation of deterministic serial startup orchestration.
 */

#include "game_ex/startup/startup_graph.hpp"

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
     */
    Entry(
        std::string stable_id,
        std::vector<std::string> dependency_ids,
        std::unique_ptr<Subsystem> implementation)
        : id(std::move(stable_id)),
          dependencies(std::move(dependency_ids)),
          subsystem(std::move(implementation)) {}

    /** Stable identifier copied from the public registration. */
    const std::string id;

    /** Stable dependency identifiers copied from the public registration. */
    const std::vector<std::string> dependencies;

    /** Concrete subsystem with ordinary unique ownership. */
    std::unique_ptr<Subsystem> subsystem;
};

StartupGraph::StartupGraph() = default;

StartupGraph::~StartupGraph() {
    if (state_ == LifecycleState::running) {
        state_ = LifecycleState::stopping;
        const std::exception_ptr failure = shutdown_started();
        state_ = failure ? LifecycleState::failed : LifecycleState::stopped;
    }
}

void StartupGraph::add(SubsystemRegistration registration) {
    if (state_ != LifecycleState::configuring) {
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
        std::move(registration.subsystem)));
}

void StartupGraph::start() {
    if (state_ != LifecycleState::configuring) {
        throw std::logic_error("Startup graph can only start from the configuring state");
    }

    const std::vector<std::size_t> order = validated_start_order();
    state_ = LifecycleState::starting;

    try {
        for (const std::size_t index : order) {
            started_order_.push_back(index);
            entries_[index]->subsystem->start();
        }
    } catch (...) {
        const std::exception_ptr startup_failure = std::current_exception();
        const std::exception_ptr cleanup_failure = shutdown_started();
        static_cast<void>(cleanup_failure);
        state_ = LifecycleState::failed;
        std::rethrow_exception(startup_failure);
    }

    state_ = LifecycleState::running;
}

void StartupGraph::shutdown() {
    if (state_ != LifecycleState::running) {
        throw std::logic_error("Startup graph can only shut down from the running state");
    }

    state_ = LifecycleState::stopping;
    const std::exception_ptr failure = shutdown_started();
    state_ = failure ? LifecycleState::failed : LifecycleState::stopped;

    if (failure) {
        std::rethrow_exception(failure);
    }
}

LifecycleState StartupGraph::state() const noexcept {
    return state_;
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

} // namespace game_ex::startup
