#pragma once
#include <memory>
#include <vector>
#include <cstdint>
#include "engine/renderer/IRenderer.h"

/**
 * @brief Demo that renders a triangle.
 * 
 */
class HelloTriangleGame
{
public:
	/**
	 * @brief Initialize GPU resources for the demo.
	 * @param renderer Renderer interface.
	 * @return true on success.
	 */

	bool initialize(IRenderer& renderer);

	/**
	 * @brief Per-frame update & draw.
	 */
	void tick(IRenderer& renderer);

	/**
	 * @brief Release demo recsources.
	 */
	void shutdown(IRenderer& renderer);

private:
	IRenderer::ShaderHandle		m_vs{};
	IRenderer::ShaderHandle		m_fs{};
	IRenderer::PipelineHandle	m_pipeline{};
	IRenderer::BufferHandle		m_vbo{};
	uint32_t					m_vertexCount = 0;

	static std::vector<uint8_t> readFileBytes(const char* path);
};
