#pragma once
#include <cstdint>
#include <vector>

class IWindow;

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
