/**
 * @file application.cpp
 * @brief Implementation of the minimal application lifecycle.
 */

#include "game_ex/core/application.hpp"

#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace game_ex::core {
namespace {

/** Stable dependency identifier for the application renderer. */
constexpr std::string_view renderer_subsystem_id{"render.backend"};

/** Stable dependency identifier for native window visibility. */
constexpr std::string_view window_visibility_subsystem_id{"platform.window.visibility"};

/**
 * @brief Reversible startup adapter for the Application-owned renderer.
 */
class RendererSubsystem final : public startup::Subsystem {
public:
    /**
     * @brief Binds the subsystem to an Application-owned renderer.
     * @param renderer Renderer that outlives this startup graph entry.
     */
    explicit RendererSubsystem(render::Renderer& renderer) noexcept : renderer_(renderer) {}

    /** @copydoc game_ex::startup::Subsystem::start */
    void start() override {
        renderer_.start();
        renderer_.render_frame(render::foundation_diagnostic_frame());
    }

    /** @copydoc game_ex::startup::Subsystem::shutdown */
    void shutdown() override {
        renderer_.shutdown();
    }

private:
    /** Renderer owned by the containing Application. */
    render::Renderer& renderer_;
};

/**
 * @brief Reversible startup adapter for the application's native window.
 */
class WindowVisibilitySubsystem final : public startup::Subsystem {
public:
    /**
     * @brief Binds the subsystem to an Application-owned window.
     * @param window Window that outlives this startup graph entry.
     */
    explicit WindowVisibilitySubsystem(platform::Window& window) noexcept : window_(window) {}

    /** @copydoc game_ex::startup::Subsystem::start */
    void start() override {
        window_.show();
    }

    /** @copydoc game_ex::startup::Subsystem::shutdown */
    void shutdown() override {
        window_.hide();
    }

private:
    /** Window owned by the containing Application. */
    platform::Window& window_;
};

} // namespace

Application::Application(
    std::unique_ptr<platform::Platform> platform,
    const platform::WindowSpecification& window,
    const render::RendererFactory& renderer_factory,
    RunConfiguration run)
    : platform_(std::move(platform)), run_configuration_(run) {
    if (!platform_) {
        throw std::invalid_argument("Application requires a platform runtime");
    }

    if (run_configuration_.idle_sleep < std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("Application idle sleep cannot be negative");
    }

    if (run_configuration_.automatic_exit_after.has_value()
        && run_configuration_.automatic_exit_after.value() < std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("Application automatic exit duration cannot be negative");
    }

    platform::WindowSpecification renderer_window = window;
    const platform::WindowGraphicsApi required_api = renderer_factory.required_window_api();
    if (window.graphics_api != platform::WindowGraphicsApi::none
        && window.graphics_api != required_api) {
        throw std::invalid_argument(
            "Window graphics capability conflicts with the selected renderer factory");
    }
    renderer_window.graphics_api = required_api;

    window_ = platform_->create_window(renderer_window);
    if (!window_) {
        throw std::invalid_argument("Platform returned a null window");
    }

    renderer_ = renderer_factory.create(*window_);
    if (!renderer_) {
        throw std::invalid_argument("Renderer factory returned a null renderer");
    }

    if (renderer_->backend() != renderer_factory.backend()) {
        throw std::invalid_argument("Renderer factory returned the wrong backend type");
    }

    startup_graph_.add({
        std::string{renderer_subsystem_id},
        {},
        std::make_unique<RendererSubsystem>(*renderer_),
        startup::StartupAffinity::main_thread});
    startup_graph_.add({
        std::string{window_visibility_subsystem_id},
        {std::string{renderer_subsystem_id}},
        std::make_unique<WindowVisibilitySubsystem>(*window_),
        startup::StartupAffinity::main_thread});
}

Application::~Application() = default;

int Application::run() {
    startup_graph_.start(job_system_);

    const auto started_at = std::chrono::steady_clock::now();

    try {
        while (platform_->pump_events() == platform::EventPumpResult::continue_running) {
            if (run_configuration_.automatic_exit_after.has_value()) {
                const auto elapsed = std::chrono::steady_clock::now() - started_at;
                if (elapsed >= run_configuration_.automatic_exit_after.value()) {
                    break;
                }
            }

            renderer_->render_frame(render::foundation_diagnostic_frame());
            std::this_thread::sleep_for(run_configuration_.idle_sleep);
        }
    } catch (...) {
        try {
            startup_graph_.shutdown();
        } catch (...) {
            // Preserve the event-loop failure after still attempting shutdown.
        }
        throw;
    }

    startup_graph_.shutdown();
    return 0;
}

} // namespace game_ex::core
