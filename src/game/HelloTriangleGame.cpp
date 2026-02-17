#include "HelloTriangleGame.h"
#include <fstream>

/**
 * @brief Read an entire file into memory.
 */
std::vector<uint8_t> HelloTriangleGame::readFileBytes(const char* path)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) return{};
	f.seekg(0, std::ios::end);
	const auto size = static_cast<size_t>(f.tellg());
	f.seekg(0, std::ios::beg);
	std::vector<uint8_t> data(size);
	f.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
	return data;
}

bool HelloTriangleGame::initialize(IRenderer& renderer)
{
	// Triangle vertices in NDC with per-vertex color.
	const IRenderer::VertexPC verts[] = {
		{  0.0f, -0.5f,  1.0f, 0.2f, 0.2f },
		{  0.5f,  0.5f,  0.2f, 1.0f, 0.2f },
		{ -0.5f,  0.5f,  0.2f, 0.2f, 1.0f },
	};
	m_vertexCount = 3;

	// NOTE: For Vulkan you typically use SPIR-V. If tri.vert/tri.frag are GLSL,
	// compile them during build to .spv and load those here.
	const auto vsBytes = readFileBytes("assets/shaders/tri.vert.spv");
	const auto fsBytes = readFileBytes("assets/shaders/tri.frag.spv");
	if (vsBytes.empty() || fsBytes.empty()) return false;

	m_vs = renderer.createShader(IRenderer::ShaderStage::Vertex, vsBytes);
	m_fs = renderer.createShader(IRenderer::ShaderStage::Fragment, fsBytes);
	m_pipeline = renderer.createTrianglePipeline(m_vs, m_fs);
	m_vbo = renderer.createVertexBuffer(verts, m_vertexCount);

	return true;
}

void HelloTriangleGame::tick(IRenderer& renderer)
{
	if (!renderer.beginFrame())
		return;

	renderer.drawTriangle(m_pipeline, m_vbo, m_vertexCount);

	renderer.endFrame();
}

void HelloTriangleGame::shutdown(IRenderer& /*renderer*/)
{
	// Optional: no-op if backend frees everything on renderer.shutdown().
}
