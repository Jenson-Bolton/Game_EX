/**
 * @file application.hpp
 * @brief Minimal engine application lifecycle.
 */

#pragma once

#include "game_ex/jobs/job_system.hpp"
#include "game_ex/platform/platform.hpp"
#include "game_ex/render/renderer_factory.hpp"
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

    /** Simple frame-pacing sleep used by the diagnostic rendering loop. */
    std::chrono::milliseconds idle_sleep{8};
};

/**
 * @brief Owns the platform runtime and drives the top-level application loop.
 *
 * Application is the first composition boundary, not a service locator. The
 * executable selects a concrete Platform implementation and transfers ownership
 * to this object. A short-lived RendererFactory creates an ordinarily owned
 * renderer for the same window. The startup graph controls reversible runtime
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
     * @param renderer_factory Concrete renderer composition selected by the caller.
     * @param run Main-loop timing and optional smoke-test lifetime.
     * @throws std::invalid_argument if composition inputs or run are invalid.
     * @throws std::system_error if the bootstrap worker pool cannot create a thread.
     * @throws std::runtime_error if the platform cannot create the required window.
     * @throws render::RendererError if the renderer rejects the created window.
     * @throws std::bad_alloc if owned runtime objects cannot be allocated.
     */
    Application(
        std::unique_ptr<platform::Platform> platform,
        const platform::WindowSpecification& window,
        const render::RendererFactory& renderer_factory,
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

    /** Renderer destroyed before its borrowed window and owning platform. */
    std::unique_ptr<render::Renderer> renderer_;

    /** Fixed bootstrap worker pool that outlives parallel startup and shutdown. */
    jobs::JobSystem job_system_;

    /** Owned deterministic lifecycle for application runtime subsystems. */
    startup::StartupGraph startup_graph_;

    /** Main-loop configuration copied at construction. */
    RunConfiguration run_configuration_;
};

} // namespace game_ex::core
