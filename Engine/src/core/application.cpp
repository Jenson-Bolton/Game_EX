/**
 * @file application.cpp
 * @brief Implementation of the minimal application lifecycle.
 */

#include "game_ex/core/application.hpp"

#include <stdexcept>
#include <thread>
#include <utility>

namespace game_ex::core {
namespace {

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

    /** @copydoc startup::Subsystem::start */
    void start() override {
        window_.show();
    }

    /** @copydoc startup::Subsystem::shutdown */
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

    window_ = platform_->create_window(window);
    startup_graph_.add({
        "platform.window.visibility",
        {},
        std::make_unique<WindowVisibilitySubsystem>(*window_)});
}

Application::~Application() = default;

int Application::run() {
    startup_graph_.start();

    const auto started_at = std::chrono::steady_clock::now();

    try {
        while (platform_->pump_events() == platform::EventPumpResult::continue_running) {
            if (run_configuration_.automatic_exit_after.has_value()) {
                const auto elapsed = std::chrono::steady_clock::now() - started_at;
                if (elapsed >= run_configuration_.automatic_exit_after.value()) {
                    break;
                }
            }

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
