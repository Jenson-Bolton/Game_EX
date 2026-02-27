#pragma once

#include <memory>

#include "platform/window/IWindow.h"

namespace platform::window
{
	/**
	 * @brief Create a window.
	 * 
	 * Platform agnotic create window function.
	 * 
	 * @param wd Window description.
	 */
	std::unique_ptr<IWindow> create_window(const WindowDesc& wd);
}
