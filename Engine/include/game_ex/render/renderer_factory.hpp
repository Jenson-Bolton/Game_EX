/**
 * @file renderer_factory.hpp
 * @brief Backend-neutral renderer composition boundary.
 */

#pragma once

#include "game_ex/platform/window.hpp"
#include "game_ex/render/renderer.hpp"

#include <memory>

namespace game_ex::render {

/**
 * @brief Creates a dormant concrete renderer for a compatible owned window.
 *
 * The factory is passed into Application only during construction. It is not a
 * service locator and need not outlive the returned renderer.
 *
 * @ingroup render
 */
class RendererFactory {
public:
    /** Enables destruction through the backend-neutral factory interface. */
    virtual ~RendererFactory() = default;

    /** Factories are ordinary composition objects and are not copied. */
    RendererFactory(const RendererFactory&) = delete;

    /** Factories cannot be copy-assigned. */
    RendererFactory& operator=(const RendererFactory&) = delete;

    /**
     * @brief Identifies the concrete backend produced by this factory.
     * @return Concrete renderer backend.
     */
    [[nodiscard]] virtual RendererBackend backend() const noexcept = 0;

    /**
     * @brief Reports the native window capability required before creation.
     * @return Platform-neutral graphics API flag.
     */
    [[nodiscard]] virtual platform::WindowGraphicsApi required_window_api() const noexcept = 0;

    /**
     * @brief Creates one dormant renderer bound to an Application-owned window.
     * @param window Window that must outlive the returned renderer.
     * @return Uniquely owned dormant renderer; never null on success.
     * @throws RendererError if the window is not a compatible backend window.
     * @throws std::bad_alloc if renderer allocation fails.
     */
    [[nodiscard]] virtual std::unique_ptr<Renderer> create(platform::Window& window) const = 0;

protected:
    /** Allows construction only by concrete renderer factories. */
    RendererFactory() = default;
};

} // namespace game_ex::render
