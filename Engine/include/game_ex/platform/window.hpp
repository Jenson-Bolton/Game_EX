/**
 * @file window.hpp
 * @brief Platform-neutral desktop window types.
 */

#pragma once

#include <cstdint>
#include <string>

namespace game_ex::platform {

/**
 * @brief Describes a top-level application window before it is created.
 *
 * The dimensions are expressed in window coordinates rather than framebuffer
 * pixels so that the platform backend can apply the operating system's DPI
 * policy.
 *
 * @ingroup platform
 */
struct WindowSpecification final {
    /** Text displayed by the operating system in the title bar. */
    std::string title;

    /** Requested client-area width in window coordinates. */
    std::uint32_t width{1280};

    /** Requested client-area height in window coordinates. */
    std::uint32_t height{720};

    /** Whether the user may resize the window. */
    bool resizable{true};
};

/**
 * @brief Platform-neutral ownership interface for a top-level window.
 *
 * A Window owns the corresponding native window. It must be destroyed before
 * the Platform instance that created it.
 *
 * @ingroup platform
 */
class Window {
public:
    /** Enables correct destruction through the platform-neutral interface. */
    virtual ~Window() = default;

    /** Window objects are uniquely owned native resources. */
    Window(const Window&) = delete;

    /** Window objects cannot be copy-assigned. */
    Window& operator=(const Window&) = delete;

    /**
     * @brief Makes the native window visible.
     * @throws std::runtime_error if the operating system rejects the request.
     */
    virtual void show() = 0;

    /**
     * @brief Makes the native window hidden.
     * @throws std::runtime_error if the operating system rejects the request.
     */
    virtual void hide() = 0;

protected:
    /** Allows construction only by concrete platform implementations. */
    Window() = default;
};

} // namespace game_ex::platform
