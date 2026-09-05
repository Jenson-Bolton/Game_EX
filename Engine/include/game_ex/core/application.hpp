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
#include <cstdint>
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
     * leaves this empty and runs until the user closes its window. A timed run
     * succeeds only after at least one frame was actually presented.
     */
    std::optional<std::chrono::milliseconds> automatic_exit_after;

    /** Simple frame-pacing sleep used by the diagnostic rendering loop. */
    std::chrono::milliseconds idle_sleep{8};

    /** Owning presentation copied into Application and then reused immutably. */
    render::RenderFrame render_frame{render::foundation_render_frame()};
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
     * @param run Owned presentation, main-loop timing, and optional smoke lifetime.
     * @throws std::invalid_argument if composition inputs or run are invalid.
     * @throws std::system_error if the bootstrap worker pool cannot create a thread.
     * @throws render::RendererError if native window creation or renderer setup fails.
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

    /**
     * @brief Reports whether this application has ever shown its native window.
     * @return True after the visibility subsystem completed startup.
     *
     * This monotonic evidence lets renderer auto-selection distinguish a safe
     * pre-visibility initialization fallback from a visible runtime failure.
     */
    [[nodiscard]] bool reached_window_visibility() const noexcept;

private:
    /** Platform runtime; declared before window so it is destroyed after it. */
    std::unique_ptr<platform::Platform> platform_;

    /** The current milestone's sole top-level application window. */
    std::unique_ptr<platform::Window> window_;

    /** Renderer destroyed before its borrowed window and owning platform. */
    std::unique_ptr<render::Renderer> renderer_;

    /** Fixed bootstrap worker pool that outlives parallel startup and shutdown. */
    jobs::JobSystem job_system_;

    /** Immutable main-loop configuration owned for the application's lifetime. */
    const RunConfiguration run_configuration_;

    /** Owned deterministic lifecycle for application runtime subsystems. */
    startup::StartupGraph startup_graph_;

    /** Frames confirmed as presented by the renderer during this run. */
    std::uint64_t presented_frames_{};

    /** Monotonic evidence that native window visibility completed startup. */
    bool reached_window_visibility_{};

};

} // namespace game_ex::core
