/**
 * @file application.hpp
 * @brief Minimal engine application lifecycle.
 */

#pragma once

#include "game_ex/platform/platform.hpp"
#include "game_ex/startup/startup_graph.hpp"

#include <chrono>
#include <memory>
#include <optional>

namespace game_ex::core {

/**
 * @brief Configures behaviour of the foundation-stage application loop.
 * @ingroup core
 */
struct RunConfiguration final {
    /**
     * Optional lifetime used by automated smoke tests. A normal application
     * leaves this empty and runs until the user closes its window.
     */
    std::optional<std::chrono::milliseconds> automatic_exit_after;

    /** Sleep duration used while the foundation has no update/render workload. */
    std::chrono::milliseconds idle_sleep{8};
};

/**
 * @brief Owns the platform runtime and drives the top-level application loop.
 *
 * Application is the first composition boundary, not a service locator. The
 * executable selects a concrete Platform implementation and transfers ownership
 * to this object. Its ordinarily owned startup graph controls reversible runtime
 * activation without exposing global subsystem lookup.
 *
 * @ingroup core
 */
class Application final {
public:
    /**
     * @brief Creates the application's single foundation-stage window.
     * @param platform Initialised platform runtime selected by the composition root.
     * @param window Requested window properties.
     * @param run Main-loop timing and optional smoke-test lifetime.
     * @throws std::invalid_argument if platform is null or run is invalid.
     * @throws std::runtime_error if the platform cannot create the window.
     */
    Application(
        std::unique_ptr<platform::Platform> platform,
        const platform::WindowSpecification& window,
        RunConfiguration run = {});

    /** Shuts down active subsystems before releasing the window and platform. */
    ~Application();

    /** Applications have unique ownership of platform resources. */
    Application(const Application&) = delete;

    /** Applications cannot be copy-assigned. */
    Application& operator=(const Application&) = delete;

    /**
     * @brief Starts subsystems and processes events until shutdown is requested.
     * @return Zero after an orderly shutdown.
     * @throws std::logic_error if this single-use application has already run.
     * @throws Any startup or shutdown exception from an application subsystem.
     */
    int run();

private:
    /** Platform runtime; declared before window so it is destroyed after it. */
    std::unique_ptr<platform::Platform> platform_;

    /** The current milestone's sole top-level application window. */
    std::unique_ptr<platform::Window> window_;

    /** Owned deterministic lifecycle for application runtime subsystems. */
    startup::StartupGraph startup_graph_;

    /** Main-loop configuration copied at construction. */
    RunConfiguration run_configuration_;
};

} // namespace game_ex::core
