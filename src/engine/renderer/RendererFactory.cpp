#include "IRenderer.h"
#include "backends/vulkan/VulkanRenderer.h"
#include <memory>

std::unique_ptr<IRenderer> CreateRendererBackend()
{
    return std::make_unique<VulkanRenderer>();
}
