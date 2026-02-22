#include "VulkanRenderer.h"
#include "../../../../platform/IWindow.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <stdexcept>
#include <set>
#include <algorithm>
#include <iostream>
#include <string_view>

using namespace platform;

VulkanRenderer::VulkanRenderer() = default;

VulkanRenderer::~VulkanRenderer()
{
	shutdown();
}

bool VulkanRenderer::initialize(const InitInfo& info, IWindow& window)
{
	try
	{
		if (!createInstance()) { std::cerr << "Vulkan init failed at createInstance" << std::endl; return false; }
		if (!createSurface(window)) { std::cerr << "Vulkan init failed at createSurface" << std::endl; return false; }
		if (!pickPhysicalDevice()) { std::cerr << "Vulkan init failed at pickPhysicalDevice" << std::endl; return false; }
		if (!createLogicalDevice()) { std::cerr << "Vulkan init failed at createLogicalDevice" << std::endl; return false; }
		if (!createSwapchain()) { std::cerr << "Vulkan init failed at createSwapchain" << std::endl; return false; }
		if (!createImageViews()) { std::cerr << "Vulkan init failed at createImageViews" << std::endl; return false; }
		if (!createRenderPass()) { std::cerr << "Vulkan init failed at createRenderPass" << std::endl; return false; }
		if (!createFramebuffers()) { std::cerr << "Vulkan init failed at createFramebuffers" << std::endl; return false; }
		if (!createCommandPool()) { std::cerr << "Vulkan init failed at createCommandPool" << std::endl; return false; }
		if (!createCommandBuffers()) { std::cerr << "Vulkan init failed at createCommandBuffers" << std::endl; return false; }
		if (!createSyncObjects()) { std::cerr << "Vulkan init failed at createSyncObjects" << std::endl; return false; }

		return true;
	}
	catch (const std::exception& e)
	{
		std::cerr << "Vulkan exception during initialize: " << e.what() << std::endl;
		return false;
	}
}

void VulkanRenderer::shutdown()
{
	if (m_device)
	{
		m_device.waitIdle();

		// Clean up buffers
		for (auto& buffer : m_buffers)
		{
			if (buffer->buffer) m_device.destroyBuffer(buffer->buffer);
			if (buffer->memory) m_device.freeMemory(buffer->memory);
		}
		m_buffers.clear();

		// Clean up pipelines
		for (auto& pipeline : m_pipelines)
		{
			if (pipeline->pipeline) m_device.destroyPipeline(pipeline->pipeline);
			if (pipeline->layout) m_device.destroyPipelineLayout(pipeline->layout);
		}
		m_pipelines.clear();

		// Clean up shaders
		for (auto& shader : m_shaders)
		{
			if (shader->module) m_device.destroyShaderModule(shader->module);
		}
		m_shaders.clear();

		// Clean up sync objects
		if (m_imageAvailableSemaphore) m_device.destroySemaphore(m_imageAvailableSemaphore);
		if (m_renderFinishedSemaphore) m_device.destroySemaphore(m_renderFinishedSemaphore);
		if (m_inFlightFence) m_device.destroyFence(m_inFlightFence);

		// Clean up command buffers and pool
		if (m_commandPool)
		{
			m_device.freeCommandBuffers(m_commandPool, m_commandBuffers);
			m_device.destroyCommandPool(m_commandPool);
		}

		// Clean up framebuffers
		for (auto framebuffer : m_framebuffers)
		{
			m_device.destroyFramebuffer(framebuffer);
		}
		m_framebuffers.clear();

		// Clean up render pass
		if (m_renderPass) m_device.destroyRenderPass(m_renderPass);

		// Clean up image views
		for (auto imageView : m_swapchainImageViews)
		{
			m_device.destroyImageView(imageView);
		}
		m_swapchainImageViews.clear();

		// Clean up swapchain
		if (m_swapchain) m_device.destroySwapchainKHR(m_swapchain);

		// Clean up surface
		if (m_surface) m_instance.destroySurfaceKHR(m_surface);

		// Clean up device
		m_device.destroy();

		// Clean up instance
		m_instance.destroy();
	}
}

bool VulkanRenderer::beginFrame()
{
	// Wait for previous frame
	vk::Result result = m_device.waitForFences(1, &m_inFlightFence, VK_TRUE, UINT64_MAX);
	if (result != vk::Result::eSuccess) return false;

	m_device.resetFences(1, &m_inFlightFence);

	// Acquire next image
	result = m_device.acquireNextImageKHR(m_swapchain, UINT64_MAX, m_imageAvailableSemaphore, nullptr, &m_imageIndex);
	if (result == vk::Result::eErrorOutOfDateKHR)
	{
		// Recreate swapchain
		return false;
	}
	else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
	{
		return false;
	}

	// Reset command buffer
	m_commandBuffers[m_currentFrame].reset();

	return true;
}

void VulkanRenderer::endFrame()
{
	// Submit command buffer
	vk::SubmitInfo submitInfo{};
	vk::Semaphore waitSemaphores[] = { m_imageAvailableSemaphore };
	vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_commandBuffers[m_currentFrame];
	vk::Semaphore signalSemaphores[] = { m_renderFinishedSemaphore };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	m_graphicsQueue.submit(1, &submitInfo, m_inFlightFence);

	// Present
	vk::PresentInfoKHR presentInfo{};
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	vk::SwapchainKHR swapchains[] = { m_swapchain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapchains;
	presentInfo.pImageIndices = &m_imageIndex;

	m_graphicsQueue.presentKHR(presentInfo);

	m_currentFrame = (m_currentFrame + 1) % 1; // Single frame in flight
}

IRenderer::ShaderHandle VulkanRenderer::createShader(ShaderStage stage, const std::vector<uint8_t>& bytecode)
{
	vk::ShaderModuleCreateInfo createInfo{};
	createInfo.codeSize = bytecode.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(bytecode.data());

	vk::ShaderModule shaderModule = m_device.createShaderModule(createInfo);

	auto shader = std::make_unique<VulkanShader>();
	shader->module = shaderModule;
	shader->stage = stage;

	m_shaders.push_back(std::move(shader));
	return { static_cast<uint64_t>(m_shaders.size() - 1) };
}

IRenderer::PipelineHandle VulkanRenderer::createTrianglePipeline(ShaderHandle vs, ShaderHandle fs)
{
	auto& vertexShader = m_shaders[vs.id];
	auto& fragmentShader = m_shaders[fs.id];

	vk::PipelineShaderStageCreateInfo shaderStages[2];

	shaderStages[0].stage = vk::ShaderStageFlagBits::eVertex;
	shaderStages[0].module = vertexShader->module;
	shaderStages[0].pName = "main";

	shaderStages[1].stage = vk::ShaderStageFlagBits::eFragment;
	shaderStages[1].module = fragmentShader->module;
	shaderStages[1].pName = "main";

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
	auto bindingDescription = VertexPC::getBindingDescription();
	auto attributeDescriptions = VertexPC::getAttributeDescriptions();
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

	vk::Viewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_swapchainImages.size() > 0 ? 800 : 800); // Default size
	viewport.height = static_cast<float>(m_swapchainImages.size() > 0 ? 600 : 600);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	vk::Rect2D scissor{};
	scissor.offset = vk::Offset2D{ 0, 0 };
	scissor.extent = vk::Extent2D{ 800, 600 };

	vk::PipelineViewportStateCreateInfo viewportState{};
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	vk::PipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = vk::PolygonMode::eFill;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = vk::CullModeFlagBits::eBack;
	rasterizer.frontFace = vk::FrontFace::eClockwise;
	rasterizer.depthBiasEnable = VK_FALSE;

	vk::PipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

	vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	colorBlendAttachment.blendEnable = VK_FALSE;

	vk::PipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
	vk::PipelineLayout pipelineLayout = m_device.createPipelineLayout(pipelineLayoutInfo);

	vk::GraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = m_renderPass;
	pipelineInfo.subpass = 0;

	vk::Pipeline graphicsPipeline = m_device.createGraphicsPipeline(nullptr, pipelineInfo).value;

	auto pipeline = std::make_unique<VulkanPipeline>();
	pipeline->pipeline = graphicsPipeline;
	pipeline->layout = pipelineLayout;

	m_pipelines.push_back(std::move(pipeline));
	return { static_cast<uint64_t>(m_pipelines.size() - 1) };
}

IRenderer::BufferHandle VulkanRenderer::createVertexBuffer(const VertexPC* vertices, uint32_t vertexCount)
{
	vk::DeviceSize bufferSize = sizeof(VertexPC) * vertexCount;

	vk::Buffer stagingBuffer;
	vk::DeviceMemory stagingBufferMemory;
	createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

	void* data = m_device.mapMemory(stagingBufferMemory, 0, bufferSize);
	memcpy(data, vertices, static_cast<size_t>(bufferSize));
	m_device.unmapMemory(stagingBufferMemory);

	vk::Buffer vertexBuffer;
	vk::DeviceMemory vertexBufferMemory;
	createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, vertexBuffer, vertexBufferMemory);

	// Copy from staging to vertex buffer
	vk::CommandBuffer commandBuffer = beginSingleTimeCommands();
	vk::BufferCopy copyRegion{};
	copyRegion.size = bufferSize;
	commandBuffer.copyBuffer(stagingBuffer, vertexBuffer, 1, &copyRegion);
	endSingleTimeCommands(commandBuffer);

	m_device.destroyBuffer(stagingBuffer);
	m_device.freeMemory(stagingBufferMemory);

	auto buffer = std::make_unique<VulkanBuffer>();
	buffer->buffer = vertexBuffer;
	buffer->memory = vertexBufferMemory;
	buffer->vertexCount = vertexCount;

	m_buffers.push_back(std::move(buffer));
	return { static_cast<uint64_t>(m_buffers.size() - 1) };
}

void VulkanRenderer::drawTriangle(PipelineHandle pipeline, BufferHandle vertexBuffer, uint32_t vertexCount)
{
	auto& vkPipeline = m_pipelines[pipeline.id];
	auto& vkBuffer = m_buffers[vertexBuffer.id];

	vk::CommandBuffer commandBuffer = m_commandBuffers[m_currentFrame];

	vk::CommandBufferBeginInfo beginInfo{};
	commandBuffer.begin(beginInfo);

	vk::RenderPassBeginInfo renderPassInfo{};
	renderPassInfo.renderPass = m_renderPass;
	renderPassInfo.framebuffer = m_framebuffers[m_imageIndex];
	renderPassInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
	renderPassInfo.renderArea.extent = vk::Extent2D{ 800, 600 };

	vk::ClearValue clearColor = vk::ClearColorValue{ std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f } };
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;

	commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, vkPipeline->pipeline);

	vk::Buffer vertexBuffers[] = { vkBuffer->buffer };
	vk::DeviceSize offsets[] = { 0 };
	commandBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);

	commandBuffer.draw(vertexCount, 1, 0, 0);

	commandBuffer.endRenderPass();
	commandBuffer.end();
}

// Helper functions implementation would go here...
// (createInstance, createSurface, etc.)

bool VulkanRenderer::createInstance()
{
	vk::ApplicationInfo appInfo{};
	appInfo.pApplicationName = "Game_EX";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Game_EX Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	vk::InstanceCreateInfo createInfo{};
	createInfo.pApplicationInfo = &appInfo;

	// Get required extensions for SDL
	uint32_t extensionCount = 0;
	const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
	if (!sdlExtensions) return false;

	std::vector<const char*> extensions(sdlExtensions, sdlExtensions + extensionCount);

	const auto hasExtension = [&extensions](const char* name) {
		return std::any_of(
			extensions.begin(),
			extensions.end(),
			[name](const char* ext) { return std::string_view(ext) == name; });
	};

	const auto availableInstanceExtensions = vk::enumerateInstanceExtensionProperties();
	const auto isInstanceExtensionAvailable = [&availableInstanceExtensions](const char* name) {
		return std::any_of(
			availableInstanceExtensions.begin(),
			availableInstanceExtensions.end(),
			[name](const vk::ExtensionProperties& ext) { return std::string_view(ext.extensionName) == name; });
	};

	// Required on macOS/MoltenVK to enumerate portability physical devices.
	const bool portabilityEnumerationAvailable = isInstanceExtensionAvailable(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
	if (portabilityEnumerationAvailable && !hasExtension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
	{
		extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
	}

	// Often needed alongside portability enumeration on Vulkan 1.0 paths.
	if (isInstanceExtensionAvailable(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) &&
		!hasExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
	{
		extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
	}

	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();
	if (portabilityEnumerationAvailable)
	{
		createInfo.flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
	}

	// Enable validation layer only if available.
	std::vector<const char*> layers;
	constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";
	const auto availableLayers = vk::enumerateInstanceLayerProperties();
	const bool hasValidationLayer = std::any_of(
		availableLayers.begin(),
		availableLayers.end(),
		[](const vk::LayerProperties& p) { return std::string_view(p.layerName) == kValidationLayer; });

	if (hasValidationLayer)
	{
		layers.push_back(kValidationLayer);
		createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
		createInfo.ppEnabledLayerNames = layers.data();
	}
	else
	{
		std::cerr << "Vulkan validation layer not found; continuing without it." << std::endl;
	}

	m_instance = vk::createInstance(createInfo);
	return true;
}

bool VulkanRenderer::createSurface(IWindow& window)
{
	auto nativeHandle = window.getNativeHandle();
	if (!SDL_Vulkan_CreateSurface(
		static_cast<SDL_Window*>(nativeHandle.platformWindow),
		static_cast<VkInstance>(m_instance),
		nullptr,
		reinterpret_cast<VkSurfaceKHR*>(&m_surface)))
	{
		std::cerr << "SDL_Vulkan_CreateSurface failed: " << SDL_GetError() << std::endl;
		return false;
	}
	return true;
}

bool VulkanRenderer::pickPhysicalDevice()
{
	auto devices = m_instance.enumeratePhysicalDevices();
	if (devices.empty()) return false;

	// Just pick the first device for now
	m_physicalDevice = devices[0];
	return true;
}

bool VulkanRenderer::createLogicalDevice()
{
	// Find graphics queue family
	auto queueFamilies = m_physicalDevice.getQueueFamilyProperties();
	uint32_t graphicsFamily = UINT32_MAX;
	for (uint32_t i = 0; i < queueFamilies.size(); i++)
	{
		if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics)
		{
			graphicsFamily = i;
			break;
		}
	}
	if (graphicsFamily == UINT32_MAX) return false;

	float queuePriority = 1.0f;
	vk::DeviceQueueCreateInfo queueCreateInfo{};
	queueCreateInfo.queueFamilyIndex = graphicsFamily;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	vk::PhysicalDeviceFeatures deviceFeatures{};

	vk::DeviceCreateInfo createInfo{};
	createInfo.queueCreateInfoCount = 1;
	createInfo.pQueueCreateInfos = &queueCreateInfo;
	createInfo.pEnabledFeatures = &deviceFeatures;

	// Enable swapchain extension
	std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	constexpr const char* kPortabilitySubsetExt = "VK_KHR_portability_subset";
	const auto availableDeviceExtensions = m_physicalDevice.enumerateDeviceExtensionProperties();
	const bool hasPortabilitySubset = std::any_of(
		availableDeviceExtensions.begin(),
		availableDeviceExtensions.end(),
		[](const vk::ExtensionProperties& ext) {
			return std::string_view(ext.extensionName) == kPortabilitySubsetExt;
		});
	if (hasPortabilitySubset)
	{
		deviceExtensions.push_back(kPortabilitySubsetExt);
	}
	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

	m_device = m_physicalDevice.createDevice(createInfo);
	m_graphicsQueue = m_device.getQueue(graphicsFamily, 0);

	return true;
}

bool VulkanRenderer::createSwapchain()
{
	// Simplified swapchain creation
	vk::SwapchainCreateInfoKHR createInfo{};
	createInfo.surface = m_surface;
	createInfo.minImageCount = 2;
	createInfo.imageFormat = vk::Format::eB8G8R8A8Unorm;
	createInfo.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
	createInfo.imageExtent = vk::Extent2D{ 800, 600 };
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
	createInfo.imageSharingMode = vk::SharingMode::eExclusive;
	createInfo.preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
	createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	createInfo.presentMode = vk::PresentModeKHR::eFifo;
	createInfo.clipped = VK_TRUE;

	m_swapchain = m_device.createSwapchainKHR(createInfo);
	m_swapchainImages = m_device.getSwapchainImagesKHR(m_swapchain);

	return true;
}

bool VulkanRenderer::createImageViews()
{
	m_swapchainImageViews.resize(m_swapchainImages.size());
	for (size_t i = 0; i < m_swapchainImages.size(); i++)
	{
		vk::ImageViewCreateInfo createInfo{};
		createInfo.image = m_swapchainImages[i];
		createInfo.viewType = vk::ImageViewType::e2D;
		createInfo.format = vk::Format::eB8G8R8A8Unorm;
		createInfo.components.r = vk::ComponentSwizzle::eIdentity;
		createInfo.components.g = vk::ComponentSwizzle::eIdentity;
		createInfo.components.b = vk::ComponentSwizzle::eIdentity;
		createInfo.components.a = vk::ComponentSwizzle::eIdentity;
		createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		m_swapchainImageViews[i] = m_device.createImageView(createInfo);
	}
	return true;
}

bool VulkanRenderer::createRenderPass()
{
	vk::AttachmentDescription colorAttachment{};
	colorAttachment.format = vk::Format::eB8G8R8A8Unorm;
	colorAttachment.samples = vk::SampleCountFlagBits::e1;
	colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
	colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
	colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
	colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

	vk::AttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

	vk::SubpassDescription subpass{};
	subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	vk::RenderPassCreateInfo renderPassInfo{};
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	m_renderPass = m_device.createRenderPass(renderPassInfo);
	return true;
}

bool VulkanRenderer::createFramebuffers()
{
	m_framebuffers.resize(m_swapchainImageViews.size());
	for (size_t i = 0; i < m_swapchainImageViews.size(); i++)
	{
		vk::ImageView attachments[] = { m_swapchainImageViews[i] };

		vk::FramebufferCreateInfo framebufferInfo{};
		framebufferInfo.renderPass = m_renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = 800;
		framebufferInfo.height = 600;
		framebufferInfo.layers = 1;

		m_framebuffers[i] = m_device.createFramebuffer(framebufferInfo);
	}
	return true;
}

bool VulkanRenderer::createCommandPool()
{
	// Find graphics queue family
	auto queueFamilies = m_physicalDevice.getQueueFamilyProperties();
	uint32_t graphicsFamily = UINT32_MAX;
	for (uint32_t i = 0; i < queueFamilies.size(); i++)
	{
		if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics)
		{
			graphicsFamily = i;
			break;
		}
	}

	vk::CommandPoolCreateInfo poolInfo{};
	poolInfo.queueFamilyIndex = graphicsFamily;

	m_commandPool = m_device.createCommandPool(poolInfo);
	return true;
}

bool VulkanRenderer::createCommandBuffers()
{
	m_commandBuffers.resize(m_framebuffers.size());

	vk::CommandBufferAllocateInfo allocInfo{};
	allocInfo.commandPool = m_commandPool;
	allocInfo.level = vk::CommandBufferLevel::ePrimary;
	allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

	m_commandBuffers = m_device.allocateCommandBuffers(allocInfo);
	return true;
}

bool VulkanRenderer::createSyncObjects()
{
	vk::SemaphoreCreateInfo semaphoreInfo{};
	vk::FenceCreateInfo fenceInfo{};
	fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled; // Start signaled

	m_imageAvailableSemaphore = m_device.createSemaphore(semaphoreInfo);
	m_renderFinishedSemaphore = m_device.createSemaphore(semaphoreInfo);
	m_inFlightFence = m_device.createFence(fenceInfo);

	return true;
}

bool VulkanRenderer::createBuffer(size_t size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties,
	vk::Buffer& buffer, vk::DeviceMemory& bufferMemory)
{
	vk::BufferCreateInfo bufferInfo{};
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = vk::SharingMode::eExclusive;

	buffer = m_device.createBuffer(bufferInfo);

	vk::MemoryRequirements memRequirements = m_device.getBufferMemoryRequirements(buffer);
	vk::MemoryAllocateInfo allocInfo{};
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	bufferMemory = m_device.allocateMemory(allocInfo);
	m_device.bindBufferMemory(buffer, bufferMemory, 0);

	return true;
}

uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
	vk::PhysicalDeviceMemoryProperties memProperties = m_physicalDevice.getMemoryProperties();

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	throw std::runtime_error("Failed to find suitable memory type!");
}

vk::CommandBuffer VulkanRenderer::beginSingleTimeCommands()
{
	vk::CommandBufferAllocateInfo allocInfo{};
	allocInfo.level = vk::CommandBufferLevel::ePrimary;
	allocInfo.commandBufferCount = 1;
	allocInfo.commandPool = m_commandPool;

	vk::CommandBuffer commandBuffer = m_device.allocateCommandBuffers(allocInfo)[0];

	vk::CommandBufferBeginInfo beginInfo{};
	beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

	commandBuffer.begin(beginInfo);
	return commandBuffer;
}

void VulkanRenderer::endSingleTimeCommands(vk::CommandBuffer commandBuffer)
{
	commandBuffer.end();

	vk::SubmitInfo submitInfo{};
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	m_graphicsQueue.submit(1, &submitInfo, nullptr);
	m_graphicsQueue.waitIdle();

	m_device.freeCommandBuffers(m_commandPool, 1, &commandBuffer);
}
