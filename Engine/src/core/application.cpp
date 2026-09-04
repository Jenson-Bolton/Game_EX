/**
 * @file application.cpp
 * @brief Implementation of the minimal application lifecycle.
 */

#include "game_ex/core/application.hpp"

#include <stdexcept>
#include <thread>
#include <utility>

namespace game_ex::core {

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
}

Application::~Application() = default;

int Application::run() {
    window_->show();

    const auto started_at = std::chrono::steady_clock::now();

    while (platform_->pump_events() == platform::EventPumpResult::continue_running) {
        if (run_configuration_.automatic_exit_after.has_value()) {
            const auto elapsed = std::chrono::steady_clock::now() - started_at;
            if (elapsed >= run_configuration_.automatic_exit_after.value()) {
                break;
            }
        }

        std::this_thread::sleep_for(run_configuration_.idle_sleep);
    }

    return 0;
}

} // namespace game_ex::core
