#pragma once
#include <cstdint>
#include <vector>

#ifdef __cplusplus
#include <vulkan/vulkan.hpp>
#endif

// Include IWindow definition
#include "../../platform/IWindow.h"

using namespace platform;

/**
 * @brief Renderer Interface.
 * 
 * The engine & game depend only on this interface.
 * Backends (Vulkan, B3D12, Metal) implement these calls.
 */
class IRenderer
{
public:
	virtual ~IRenderer() = default;

	/**
	 * @brief Initialization paramters for the renderer.
	 */
	struct InitInfo
	{
		bool enableValidation = true; ///< Enable debug layers where supported.
	};

	/**
	 * @brief Shader stage enumeration.
	 */
	enum class ShaderStage : uint8_t
	{
		Vertex,
		Fragment
	};

	/**
	 * @brief Opaque GPU resource handles owned by the backend.
	 */
	struct ShaderHandle { uint64_t id = 0; };
	struct PipelineHandle { uint64_t id = 0; };
	struct BufferHandle { uint64_t id = 0; };

	/**
	 * @brief Vertex format for the hello-triangle demo (pos + color).
	 */
	struct VertexPC
	{
		float px, py;
		float r, g, b;

		static vk::VertexInputBindingDescription getBindingDescription()
		{
			vk::VertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(VertexPC);
			bindingDescription.inputRate = vk::VertexInputRate::eVertex;
			return bindingDescription;
		}

		static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
		{
			std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions{};

			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = vk::Format::eR32G32Sfloat;
			attributeDescriptions[0].offset = offsetof(VertexPC, px);

			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
			attributeDescriptions[1].offset = offsetof(VertexPC, r);

			return attributeDescriptions;
		}
	};

	/**
	 * @brief Create the renderer using the given window.
	 */
	virtual bool initialize(const InitInfo& info, IWindow& window) = 0;

	/**
	 * @brief Shutdown and release all GPU resources.
	 */
	virtual void shutdown() = 0;

	/**
	 * @brief Begin rendering a new frame.
	 * @return false if the frame cannot be started (e.g. minimized).
	 */
	virtual bool beginFrame() = 0;

	/**
	 * @brief End rendering and present.
	 */
	virtual void endFrame() = 0;

	/**
	 * @brief Create a shader drom backend-specific bytecode.
	 *
	 * For Vulkan, this is SPIR-V.
	 */
	virtual ShaderHandle createShader(ShaderStage stage, const std::vector<uint8_t>& bytecode) = 0;

	/**
	 * @brief Simple rendering pipeline for a coloured triangle.
	 * 
	 * Keeping the demo small.
	 */
	virtual PipelineHandle createTrianglePipeline(ShaderHandle vs, ShaderHandle fs) = 0;

	/**
	 * @brief Create and immutable vertex buffer.
	 */
	virtual BufferHandle createVertexBuffer(const VertexPC* vertices, uint32_t vertexCount) = 0;

	/**
	 * @brief Record a ddraw of the given vertex buffer using the provide pipeline.
	 */
	virtual void drawTriangle(PipelineHandle pipeline, BufferHandle vertexBuffer, uint32_t vertexCount) = 0;
};
