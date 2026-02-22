#pragma once

#include "../../IRenderer.h"
#include "../../../../platform/IWindow.h"
#include <vulkan/vulkan.hpp>
#include <vector>
#include <memory>

class VulkanRenderer : public IRenderer
{
public:
	VulkanRenderer();
	~VulkanRenderer() override;

	bool initialize(const InitInfo& info, IWindow& window) override;
	void shutdown() override;
	bool beginFrame() override;
	void endFrame() override;

	ShaderHandle createShader(ShaderStage stage, const std::vector<uint8_t>& bytecode) override;
	PipelineHandle createTrianglePipeline(ShaderHandle vs, ShaderHandle fs) override;
	BufferHandle createVertexBuffer(const VertexPC* vertices, uint32_t vertexCount) override;
	void drawTriangle(PipelineHandle pipeline, BufferHandle vertexBuffer, uint32_t vertexCount) override;

private:
	struct VulkanShader
	{
		vk::ShaderModule module;
		ShaderStage stage;
	};

	struct VulkanPipeline
	{
		vk::Pipeline pipeline;
		vk::PipelineLayout layout;
	};

	struct VulkanBuffer
	{
		vk::Buffer buffer;
		vk::DeviceMemory memory;
		uint32_t vertexCount;
	};

	// Vulkan core objects
	vk::Instance m_instance;
	vk::PhysicalDevice m_physicalDevice;
	vk::Device m_device;
	vk::Queue m_graphicsQueue;
	vk::SurfaceKHR m_surface;
	vk::SwapchainKHR m_swapchain;
	std::vector<vk::Image> m_swapchainImages;
	std::vector<vk::ImageView> m_swapchainImageViews;
	vk::RenderPass m_renderPass;
	std::vector<vk::Framebuffer> m_framebuffers;
	vk::CommandPool m_commandPool;
	std::vector<vk::CommandBuffer> m_commandBuffers;
	vk::Semaphore m_imageAvailableSemaphore;
	vk::Semaphore m_renderFinishedSemaphore;
	vk::Fence m_inFlightFence;

	// Resources
	std::vector<std::unique_ptr<VulkanShader>> m_shaders;
	std::vector<std::unique_ptr<VulkanPipeline>> m_pipelines;
	std::vector<std::unique_ptr<VulkanBuffer>> m_buffers;

	uint32_t m_currentFrame = 0;
	uint32_t m_imageIndex = 0;

	// Helper functions
	bool createInstance();
	bool createSurface(IWindow& window);
	bool pickPhysicalDevice();
	bool createLogicalDevice();
	bool createSwapchain();
	bool createImageViews();
	bool createRenderPass();
	bool createFramebuffers();
	bool createCommandPool();
	bool createCommandBuffers();
	bool createSyncObjects();
	bool createBuffer(size_t size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties,
		vk::Buffer& buffer, vk::DeviceMemory& bufferMemory);
	uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
	vk::CommandBuffer beginSingleTimeCommands();
	void endSingleTimeCommands(vk::CommandBuffer commandBuffer);
};
