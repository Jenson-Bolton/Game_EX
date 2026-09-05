/**
 * @file application_render_tests.cpp
 * @brief Fake-backed tests for Application renderer lifecycle ordering.
 */

#include "game_ex/core/application.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using game_ex::platform::EventPumpResult;
using game_ex::platform::Platform;
using game_ex::platform::Window;
using game_ex::platform::WindowGraphicsApi;
using game_ex::platform::WindowSpecification;
using game_ex::render::FramePresentationResult;
using game_ex::render::RenderFrame;
using game_ex::render::Renderer;
using game_ex::render::RendererBackend;
using game_ex::render::RendererDiagnostics;
using game_ex::render::RendererFactory;
using game_ex::render::RendererLifecycleState;

/**
 * @brief Mutable evidence and failure controls shared by fake objects.
 */
struct TestState final {
    /** Ordered composition, lifecycle, and destruction events. */
    std::vector<std::string> events;

    /** Graphics API copied into the platform window request. */
    WindowGraphicsApi requested_api{WindowGraphicsApi::none};

    /** Number of frames accepted by the fake renderer. */
    int rendered_frames{};

    /** Owning snapshots received by the fake renderer in presentation order. */
    std::vector<RenderFrame> received_frames;

    /** Address of the immutable frame used for the first presentation attempt. */
    const RenderFrame* first_received_frame{};

    /** Whether every attempt reused the exact Application-owned frame object. */
    bool reused_one_frame{true};

    /** Number of event-pump calls made by Application. */
    int pump_calls{};

    /** When true, renderer startup fails after entering failed state. */
    bool fail_start{};

    /** One-based frame number that raises a runtime failure, or zero for none. */
    int fail_frame{};

    /** Number of initial frames safely deferred by the fake renderer. */
    int defer_initial_frames{};

    /** When true, the factory violates its non-null result contract. */
    bool return_null_renderer{};

    /** When true, the platform violates its non-null result contract. */
    bool return_null_window{};

    /** Backend reported by the returned renderer. */
    RendererBackend returned_backend{RendererBackend::open_gl};

    /** Fake renderer state observed immediately before shutdown cleanup. */
    RendererLifecycleState state_before_shutdown{RendererLifecycleState::dormant};
};

/**
 * @brief Reports one failed assertion without aborting the test executable.
 * @param condition Assertion result.
 * @param description Human-readable invariant.
 * @return True when the assertion passed.
 */
bool check(const bool condition, const std::string& description) {
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
    }
    return condition;
}

/**
 * @brief Tests whether invoking a callable raises a selected exception type.
 * @tparam Exception Expected exception base or exact type.
 * @tparam Callable Nullary callable type.
 * @param callable Operation under test.
 * @return True only when the selected exception type was caught.
 */
template <typename Exception, typename Callable>
bool throws_exception(Callable&& callable) {
    try {
        std::invoke(std::forward<Callable>(callable));
    } catch (const Exception&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

/**
 * @brief Platform-neutral fake window that records visibility and destruction.
 */
class FakeWindow final : public Window {
public:
    /**
     * @brief Binds the window to shared test evidence.
     * @param state Evidence object that outlives this window.
     */
    explicit FakeWindow(std::shared_ptr<TestState> state) noexcept
        : state_(std::move(state)) {}

    /** Records that renderer destruction must already have occurred. */
    ~FakeWindow() override {
        state_->events.emplace_back("window.destroy");
    }

    /** @copydoc game_ex::platform::Window::show */
    void show() override {
        state_->events.emplace_back("window.show");
    }

    /** @copydoc game_ex::platform::Window::hide */
    void hide() override {
        state_->events.emplace_back("window.hide");
    }

private:
    /** Shared evidence that outlives all fake runtime objects. */
    std::shared_ptr<TestState> state_;
};

/**
 * @brief Fake platform that captures requested graphics capability and events.
 */
class FakePlatform final : public Platform {
public:
    /**
     * @brief Binds the platform to shared test evidence.
     * @param state Evidence object that outlives this platform.
     */
    explicit FakePlatform(std::shared_ptr<TestState> state) noexcept
        : state_(std::move(state)) {}

    /** Records final platform destruction after the window. */
    ~FakePlatform() override {
        state_->events.emplace_back("platform.destroy");
    }

    /** @copydoc game_ex::platform::Platform::create_window */
    [[nodiscard]] std::unique_ptr<Window> create_window(
        const WindowSpecification& specification) override {
        state_->events.emplace_back("platform.create_window");
        state_->requested_api = specification.graphics_api;
        if (state_->return_null_window) {
            return nullptr;
        }
        return std::make_unique<FakeWindow>(state_);
    }

    /** @copydoc game_ex::platform::Platform::pump_events */
    [[nodiscard]] EventPumpResult pump_events() override {
        state_->events.emplace_back("platform.pump_events");
        ++state_->pump_calls;
        return state_->pump_calls == 1
            ? EventPumpResult::continue_running
            : EventPumpResult::exit_requested;
    }

private:
    /** Shared evidence that outlives all fake runtime objects. */
    std::shared_ptr<TestState> state_;
};

/**
 * @brief Fake renderer with controllable startup and frame failures.
 */
class FakeRenderer final : public Renderer {
public:
    /**
     * @brief Binds the renderer to shared test evidence.
     * @param state Evidence object that outlives this renderer.
     */
    explicit FakeRenderer(std::shared_ptr<TestState> state) noexcept
        : state_(std::move(state)) {
        diagnostics_.backend = state_->returned_backend;
        diagnostics_.api_major = 4U;
        diagnostics_.api_minor = 6U;
        diagnostics_.api_patch = 0U;
        diagnostics_.profile = "test";
        diagnostics_.api_version = "test 4.6";
        diagnostics_.vendor = "Game_EX tests";
        diagnostics_.device = "fake renderer";
    }

    /** Records renderer destruction before its borrowed window. */
    ~FakeRenderer() override {
        state_->events.emplace_back("renderer.destroy");
    }

    /** @copydoc game_ex::render::Renderer::backend */
    [[nodiscard]] RendererBackend backend() const noexcept override {
        return state_->returned_backend;
    }

    /** @copydoc game_ex::render::Renderer::state */
    [[nodiscard]] RendererLifecycleState state() const noexcept override {
        return lifecycle_state_;
    }

    /** @copydoc game_ex::render::Renderer::start */
    void start() override {
        state_->events.emplace_back("renderer.start");
        lifecycle_state_ = RendererLifecycleState::starting;
        if (state_->fail_start) {
            lifecycle_state_ = RendererLifecycleState::failed;
            throw std::runtime_error("injected renderer start failure");
        }
        lifecycle_state_ = RendererLifecycleState::running;
    }

    /** @copydoc game_ex::render::Renderer::render_frame */
    [[nodiscard]] FramePresentationResult render_frame(
        const RenderFrame& frame) override {
        game_ex::render::validate_render_frame(frame);
        state_->events.emplace_back("renderer.frame");
        if (state_->first_received_frame == nullptr) {
            state_->first_received_frame = &frame;
        } else {
            state_->reused_one_frame = state_->reused_one_frame
                && state_->first_received_frame == &frame;
        }
        state_->received_frames.push_back(frame);
        ++state_->rendered_frames;
        if (state_->fail_frame == state_->rendered_frames) {
            lifecycle_state_ = RendererLifecycleState::failed;
            throw std::runtime_error("injected renderer frame failure");
        }
        return state_->rendered_frames <= state_->defer_initial_frames
            ? FramePresentationResult::deferred_zero_extent
            : FramePresentationResult::presented;
    }

    /** @copydoc game_ex::render::Renderer::diagnostics */
    [[nodiscard]] const RendererDiagnostics& diagnostics() const override {
        return diagnostics_;
    }

    /** @copydoc game_ex::render::Renderer::shutdown */
    void shutdown() override {
        state_->events.emplace_back("renderer.shutdown");
        state_->state_before_shutdown = lifecycle_state_;
        lifecycle_state_ = RendererLifecycleState::stopped;
    }

private:
    /** Shared evidence that outlives all fake runtime objects. */
    std::shared_ptr<TestState> state_;

    /** Fake lifecycle state used by the public observer. */
    RendererLifecycleState lifecycle_state_{RendererLifecycleState::dormant};

    /** Stable fake diagnostic values. */
    RendererDiagnostics diagnostics_{};
};

/**
 * @brief Fake renderer factory used as Application's short-lived composition input.
 */
class FakeRendererFactory final : public RendererFactory {
public:
    /**
     * @brief Binds the factory to shared test evidence.
     * @param state Evidence object that outlives this factory.
     */
    explicit FakeRendererFactory(std::shared_ptr<TestState> state) noexcept
        : state_(std::move(state)) {}

    /** @copydoc game_ex::render::RendererFactory::backend */
    [[nodiscard]] RendererBackend backend() const noexcept override {
        return RendererBackend::open_gl;
    }

    /** @copydoc game_ex::render::RendererFactory::required_window_api */
    [[nodiscard]] WindowGraphicsApi required_window_api() const noexcept override {
        return WindowGraphicsApi::open_gl;
    }

    /** @copydoc game_ex::render::RendererFactory::create */
    [[nodiscard]] std::unique_ptr<Renderer> create(Window& window) const override {
        static_cast<void>(window);
        state_->events.emplace_back("factory.create_renderer");
        if (state_->return_null_renderer) {
            return nullptr;
        }
        return std::make_unique<FakeRenderer>(state_);
    }

private:
    /** Shared evidence that outlives all fake runtime objects. */
    std::shared_ptr<TestState> state_;
};

/**
 * @brief Constructs the common zero-delay test run configuration.
 * @return Configuration that executes one rendered loop iteration.
 */
[[nodiscard]] game_ex::core::RunConfiguration test_run_configuration() noexcept {
    return {
        .automatic_exit_after = std::nullopt,
        .idle_sleep = std::chrono::milliseconds::zero(),
    };
}

/**
 * @brief Returns the ordinary window request used by fake-backed tests.
 * @return Window request with renderer capability left for Application to fill.
 */
[[nodiscard]] WindowSpecification test_window() {
    return {
        .title = "renderer lifecycle test",
        .width = 320U,
        .height = 200U,
        .resizable = false,
    };
}

/**
 * @brief Verifies startup, first frame, visibility, loop, and reverse shutdown.
 * @return True when exact lifecycle and destruction order match the contract.
 */
bool test_successful_lifecycle_order() {
    auto state = std::make_shared<TestState>();
    FakeRendererFactory factory{state};
    bool reached_visibility{};
    {
        game_ex::core::Application application{
            std::make_unique<FakePlatform>(state),
            test_window(),
            factory,
            test_run_configuration()};
        if (application.run() != 0) {
            return check(false, "successful fake application returns zero");
        }
        reached_visibility = application.reached_window_visibility();
    }

    const std::vector<std::string> expected{
        "platform.create_window",
        "factory.create_renderer",
        "renderer.start",
        "renderer.frame",
        "window.show",
        "platform.pump_events",
        "renderer.frame",
        "platform.pump_events",
        "window.hide",
        "renderer.shutdown",
        "renderer.destroy",
        "window.destroy",
        "platform.destroy",
    };

    bool passed = check(state->events == expected, "renderer/window lifecycle order is exact");
    passed &= check(reached_visibility, "successful run retains visibility evidence");
    passed &= check(
        state->requested_api == WindowGraphicsApi::open_gl,
        "Application applies the renderer factory window capability");
    passed &= check(state->rendered_frames == 2, "startup and main loop both render a frame");
    return passed;
}

/**
 * @brief Verifies Application owns and reuses one immutable complete frame.
 * @return True when caller mutation cannot affect hidden or visible attempts.
 */
bool test_owned_render_frame_reuse() {
    auto state = std::make_shared<TestState>();
    FakeRendererFactory factory{state};
    game_ex::core::RunConfiguration requested = test_run_configuration();
    requested.render_frame = {
        .background = {0.1F, 0.2F, 0.3F, 1.0F},
        .raster = game_ex::render::DiagnosticRaster{
            .columns = 2U,
            .rows = 1U,
            .display_aspect_ratio = 2.0F,
            .linear_colours = {
                {1.0F, 0.0F, 0.0F, 1.0F},
                {0.0F, 1.0F, 0.0F, 1.0F},
            },
        },
    };

    {
        game_ex::core::Application application{
            std::make_unique<FakePlatform>(state),
            test_window(),
            factory,
            requested};

        requested.render_frame.background = {0.9F, 0.9F, 0.9F, 1.0F};
        requested.render_frame.raster.reset();
        if (application.run() != 0) {
            return check(false, "custom-frame fake application returns zero");
        }
    }

    bool passed = check(
        state->received_frames.size() == 2U,
        "hidden and visible attempts both receive a complete frame");
    passed &= check(
        state->reused_one_frame,
        "hidden and visible attempts reuse one Application-owned frame object");
    for (const RenderFrame& frame : state->received_frames) {
        passed &= check(
            frame.background.red == 0.1F && frame.raster.has_value(),
            "post-construction caller mutation cannot alter the owned frame");
        if (frame.raster.has_value()) {
            passed &= check(
                frame.raster->columns == 2U
                    && frame.raster->rows == 1U
                    && frame.raster->linear_colours.size() == 2U
                    && frame.raster->linear_colours[0].red == 1.0F
                    && frame.raster->linear_colours[1].green == 1.0F,
                "owned raster metadata and row-major colours remain intact");
        }
    }
    return passed;
}

/**
 * @brief Verifies a zero-extent hidden attempt may defer until after visibility.
 * @return True when the exact attempt/show/present sequence remains intact.
 */
bool test_deferred_hidden_attempt_then_visible_present() {
    auto state = std::make_shared<TestState>();
    state->defer_initial_frames = 1;
    FakeRendererFactory factory{state};
    bool reached_visibility{};
    {
        game_ex::core::Application application{
            std::make_unique<FakePlatform>(state),
            test_window(),
            factory,
            test_run_configuration()};
        if (application.run() != 0) {
            return check(false, "deferred-start fake application returns zero");
        }
        reached_visibility = application.reached_window_visibility();
    }

    const std::vector<std::string> expected{
        "platform.create_window",
        "factory.create_renderer",
        "renderer.start",
        "renderer.frame",
        "window.show",
        "platform.pump_events",
        "renderer.frame",
        "platform.pump_events",
        "window.hide",
        "renderer.shutdown",
        "renderer.destroy",
        "window.destroy",
        "platform.destroy",
    };
    bool passed = check(
        state->events == expected,
        "hidden deferral, visibility, and visible presentation order is exact");
    passed &= check(
        reached_visibility,
        "deferred hidden attempt records later successful visibility");
    passed &= check(
        state->rendered_frames == 2,
        "one deferred hidden attempt is followed by one presented visible frame");
    return passed;
}

/**
 * @brief Verifies a timed run cannot pass without a real presentation.
 * @return True when permanent deferral becomes a typed presentation failure.
 */
bool test_timed_run_rejects_permanent_deferral() {
    auto state = std::make_shared<TestState>();
    state->defer_initial_frames = 100;
    FakeRendererFactory factory{state};
    const game_ex::core::RunConfiguration run{
        .automatic_exit_after = std::chrono::milliseconds::zero(),
        .idle_sleep = std::chrono::milliseconds::zero(),
    };
    bool typed_failure{};
    bool reached_visibility{};
    {
        game_ex::core::Application application{
            std::make_unique<FakePlatform>(state), test_window(), factory, run};
        try {
            static_cast<void>(application.run());
        } catch (const game_ex::render::RendererError& error) {
            typed_failure = error.code()
                == game_ex::render::RendererErrorCode::presentation_failed;
        }
        reached_visibility = application.reached_window_visibility();
    }

    bool passed = check(
        typed_failure,
        "timed permanent deferral reports presentation_failed");
    passed &= check(
        reached_visibility,
        "timed no-present failure remains a post-visibility failure");
    passed &= check(
        state->events == std::vector<std::string>{
            "platform.create_window",
            "factory.create_renderer",
            "renderer.start",
            "renderer.frame",
            "window.show",
            "platform.pump_events",
            "window.hide",
            "renderer.shutdown",
            "renderer.destroy",
            "window.destroy",
            "platform.destroy",
        },
        "timed permanent-deferral cleanup order is exact");
    return passed;
}

/**
 * @brief Verifies partially started renderer rollback precedes any window show.
 * @return True when failure cleanup follows the startup subsystem contract.
 */
bool test_start_failure_rollback() {
    auto state = std::make_shared<TestState>();
    state->fail_start = true;
    FakeRendererFactory factory{state};
    bool threw = false;
    bool reached_visibility{};
    {
        game_ex::core::Application application{
            std::make_unique<FakePlatform>(state),
            test_window(),
            factory,
            test_run_configuration()};
        threw = throws_exception<std::runtime_error>([&application] {
            static_cast<void>(application.run());
        });
        reached_visibility = application.reached_window_visibility();
    }

    bool passed = check(threw, "renderer startup failure reaches the caller");
    passed &= check(!reached_visibility, "start failure remains pre-visibility");
    passed &= check(
        std::find(state->events.begin(), state->events.end(), "window.show")
            == state->events.end(),
        "window remains hidden when renderer startup fails");
    passed &= check(
        std::count(state->events.begin(), state->events.end(), "renderer.shutdown") == 1,
        "failing renderer subsystem is rolled back exactly once");
    return passed;
}

/**
 * @brief Verifies an initial-frame failure rolls back the now-started renderer.
 * @return True when the window is never shown and renderer cleanup runs.
 */
bool test_initial_frame_failure_rollback() {
    auto state = std::make_shared<TestState>();
    state->fail_frame = 1;
    FakeRendererFactory factory{state};
    bool threw = false;
    bool reached_visibility{};
    {
        game_ex::core::Application application{
            std::make_unique<FakePlatform>(state),
            test_window(),
            factory,
            test_run_configuration()};
        threw = throws_exception<std::runtime_error>([&application] {
            static_cast<void>(application.run());
        });
        reached_visibility = application.reached_window_visibility();
    }

    bool passed = check(threw, "initial diagnostic frame failure reaches the caller");
    passed &= check(!reached_visibility, "initial-frame failure remains pre-visibility");
    passed &= check(
        std::find(state->events.begin(), state->events.end(), "window.show")
            == state->events.end(),
        "window is not shown before a diagnostic frame succeeds");
    passed &= check(
        std::count(state->events.begin(), state->events.end(), "renderer.shutdown") == 1,
        "initial-frame failure rolls back renderer state");
    passed &= check(
        state->state_before_shutdown == RendererLifecycleState::failed,
        "native frame failure is terminal until shutdown");
    return passed;
}

/**
 * @brief Verifies a frame-loop failure hides the window before renderer shutdown.
 * @return True when runtime failure preserves reverse dependency cleanup.
 */
bool test_runtime_frame_failure_cleanup() {
    auto state = std::make_shared<TestState>();
    state->fail_frame = 2;
    FakeRendererFactory factory{state};
    bool threw = false;
    bool reached_visibility{};
    {
        game_ex::core::Application application{
            std::make_unique<FakePlatform>(state),
            test_window(),
            factory,
            test_run_configuration()};
        threw = throws_exception<std::runtime_error>([&application] {
            static_cast<void>(application.run());
        });
        reached_visibility = application.reached_window_visibility();
    }

    const auto hide = std::find(state->events.begin(), state->events.end(), "window.hide");
    const auto shutdown = std::find(
        state->events.begin(), state->events.end(), "renderer.shutdown");
    bool passed = check(threw, "runtime frame failure reaches the caller");
    passed &= check(reached_visibility, "runtime frame failure retains visibility evidence");
    passed &= check(hide != state->events.end(), "visible window is hidden after frame failure");
    passed &= check(shutdown != state->events.end(), "renderer shuts down after frame failure");
    passed &= check(hide < shutdown, "window hide precedes renderer shutdown");
    passed &= check(
        state->state_before_shutdown == RendererLifecycleState::failed,
        "runtime native frame failure is terminal until shutdown");
    return passed;
}

/**
 * @brief Verifies constructor checks for null, wrong-backend, and conflicting input.
 * @return True when invalid composition never reaches application startup.
 */
bool test_composition_validation() {
    bool passed = true;

    auto null_window_state = std::make_shared<TestState>();
    null_window_state->return_null_window = true;
    FakeRendererFactory null_window_factory{null_window_state};
    passed &= check(
        throws_exception<std::invalid_argument>([&] {
            game_ex::core::Application application{
                std::make_unique<FakePlatform>(null_window_state),
                test_window(),
                null_window_factory,
                test_run_configuration()};
        }),
        "null platform window result is rejected before renderer creation");
    passed &= check(
        std::find(
            null_window_state->events.begin(),
            null_window_state->events.end(),
            "factory.create_renderer") == null_window_state->events.end(),
        "null window never reaches the renderer factory");

    auto null_state = std::make_shared<TestState>();
    null_state->return_null_renderer = true;
    FakeRendererFactory null_factory{null_state};
    passed &= check(
        throws_exception<std::invalid_argument>([&] {
            game_ex::core::Application application{
                std::make_unique<FakePlatform>(null_state),
                test_window(),
                null_factory,
                test_run_configuration()};
        }),
        "null renderer factory result is rejected");

    auto wrong_state = std::make_shared<TestState>();
    wrong_state->returned_backend = RendererBackend::vulkan;
    FakeRendererFactory wrong_factory{wrong_state};
    passed &= check(
        throws_exception<std::invalid_argument>([&] {
            game_ex::core::Application application{
                std::make_unique<FakePlatform>(wrong_state),
                test_window(),
                wrong_factory,
                test_run_configuration()};
        }),
        "factory result with a different backend is rejected");

    auto conflict_state = std::make_shared<TestState>();
    FakeRendererFactory conflict_factory{conflict_state};
    WindowSpecification conflict = test_window();
    conflict.graphics_api = WindowGraphicsApi::vulkan;
    passed &= check(
        throws_exception<std::invalid_argument>([&] {
            game_ex::core::Application application{
                std::make_unique<FakePlatform>(conflict_state),
                conflict,
                conflict_factory,
                test_run_configuration()};
        }),
        "conflicting caller and renderer window capabilities are rejected");
    passed &= check(
        std::find(
            conflict_state->events.begin(),
            conflict_state->events.end(),
            "platform.create_window") == conflict_state->events.end(),
        "capability conflict is rejected before native window creation");

    auto invalid_frame_state = std::make_shared<TestState>();
    FakeRendererFactory invalid_frame_factory{invalid_frame_state};
    game_ex::core::RunConfiguration invalid_run = test_run_configuration();
    invalid_run.render_frame.raster = game_ex::render::DiagnosticRaster{
        .columns = 1U,
        .rows = 1U,
        .display_aspect_ratio = 1.0F,
        .linear_colours = {},
    };
    passed &= check(
        throws_exception<std::invalid_argument>([&] {
            game_ex::core::Application application{
                std::make_unique<FakePlatform>(invalid_frame_state),
                test_window(),
                invalid_frame_factory,
                invalid_run};
        }),
        "invalid configured frame is rejected by Application construction");
    passed &= check(
        std::find(
            invalid_frame_state->events.begin(),
            invalid_frame_state->events.end(),
            "platform.create_window") == invalid_frame_state->events.end(),
        "invalid configured frame is rejected before native window creation");
    return passed;
}

} // namespace

/**
 * @brief Runs fake-backed Application renderer lifecycle tests.
 * @return EXIT_SUCCESS when every lifecycle invariant holds.
 */
int main() {
    bool passed = true;
    passed &= test_successful_lifecycle_order();
    passed &= test_owned_render_frame_reuse();
    passed &= test_deferred_hidden_attempt_then_visible_present();
    passed &= test_timed_run_rejects_permanent_deferral();
    passed &= test_start_failure_rollback();
    passed &= test_initial_frame_failure_rollback();
    passed &= test_runtime_frame_failure_cleanup();
    passed &= test_composition_validation();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
