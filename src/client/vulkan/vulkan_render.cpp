#include <voxen/client/vulkan/vulkan_render.hpp>
#include <voxen/client/vulkan/common.hpp>
#include <voxen/util/log.hpp>
#include <voxen/config.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <vector>

// TODO: get rid of this shit
#include <voxen/shaders/vert.h>
#include <voxen/shaders/frag.h>

template <typename T, std::size_t N>
constexpr std::size_t countof(T const (&)[N]) noexcept { return N; }

namespace voxen::client
{

class VulkanImpl {
public:
	VulkanImpl(GLFWwindow *w, uint32_t window_width, uint32_t window_height);
	~VulkanImpl();

	VkDevice defaultDevice() noexcept { return device; }
	VkRenderPass defaultRenderPass() noexcept { return renderpass; }
	VkPipeline defaultPipeline() noexcept { return pipeline; }

	void beginFrame();
	VkCommandBuffer currentCommandBuffer() noexcept { return command_buffers[current_items_idx]; }
	void endFrame();
private:
	VkInstance instance = VK_NULL_HANDLE;
	VkPhysicalDevice phys_device = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkQueue queue = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
	VkRenderPass renderpass = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkCommandPool command_pool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> command_buffers;

	uint32_t current_image_idx = 0;
	uint32_t current_items_idx = 0;

	struct {
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> modes;
		VkSurfaceCapabilitiesKHR caps;

		VkFormat image_format;
		VkExtent2D extent;
		std::vector<VkImage> images;
		std::vector<VkImageView> image_views;
		std::vector<VkFramebuffer> framebuffers;
		std::vector<VkSemaphore> image_avail_semaphores;
		std::vector<VkSemaphore> render_finish_semaphores;
		std::vector<VkFence> image_free_fences;
		std::vector<VkFence> items_free_fences;
	} swapchain_info;
	uint32_t queue_family_idx = UINT32_MAX;

	void requestInstanceLayers(VkInstanceCreateInfo &create_info);
	void requestDeviceExtensions(VkDeviceCreateInfo &create_info);
	bool isDeviceSuitable(VkPhysicalDevice device);
};

VulkanRender::VulkanRender(Window &w) {
	Log::info("Creating Vulkan instance");
	int w_width = w.width();
	int w_height = w.height();
	m_vk = new VulkanImpl(w.glfwHandle(), w_width, w_height);
	m_octree = new DebugDrawOctree(m_vk->defaultDevice(), m_vk->defaultRenderPass(), w_width, w_height);
}

VulkanRender::~VulkanRender() {
	Log::info("Stopping Vulkan instance");
	delete m_octree;
	delete m_vk;
}

void VulkanRender::beginFrame() {
	m_vk->beginFrame();
	//vkCmdBindPipeline(m_vk->currentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_vk->defaultPipeline());
	//vkCmdDraw(m_vk->currentCommandBuffer(), 3, 1, 0, 0);
	m_octree->beginRendering(m_vk->currentCommandBuffer());
}

void VulkanRender::debugDrawOctreeNode(glm::mat4 camera_matrix, float base_x, float base_y, float base_z, float size) {
	m_octree->drawNode(m_vk->currentCommandBuffer(), camera_matrix, base_x, base_y, base_z, size);
}

void VulkanRender::endFrame() {
	m_octree->endRendering(m_vk->currentCommandBuffer());
	m_vk->endFrame();
}

VulkanImpl::VulkanImpl(GLFWwindow *w, uint32_t window_width, uint32_t window_height) {
	auto allocator = VulkanHostAllocator::callbacks();
	/*
	 * Create VK instance
	 */
	{
		// Application info
		VkApplicationInfo app_info = {};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = "Vulkan test";
		app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
		app_info.pEngineName = "Vulkan test";
		app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
		app_info.apiVersion = VK_API_VERSION_1_1;
		// Instance create info
		VkInstanceCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.pApplicationInfo = &app_info;
		uint32_t ext_count = 0;
		const char **extensions = glfwGetRequiredInstanceExtensions(&ext_count);
		create_info.enabledExtensionCount = ext_count;
		create_info.ppEnabledExtensionNames = extensions;
		requestInstanceLayers(create_info);
		if (vkCreateInstance(&create_info, allocator, &instance) != VK_SUCCESS)
			throw std::runtime_error("failed to create Vulkan instance");
		// Create surface
		if (glfwCreateWindowSurface(instance, w, allocator, &surface) != VK_SUCCESS)
			throw std::runtime_error("failed to create window surface");
	}
	/*
	 * Find suitable physical device
	 */
	{
		uint32_t dev_count = 0;
		vkEnumeratePhysicalDevices(instance, &dev_count, nullptr);
		if (dev_count == 0)
			throw std::runtime_error("no physical devices present in the system");
		std::vector<VkPhysicalDevice> devices(dev_count);
		vkEnumeratePhysicalDevices(instance, &dev_count, devices.data());
		for (const auto &dev : devices) {
			if (isDeviceSuitable(dev)) {
				phys_device = dev;
				break;
			}
		}
		if (phys_device == VK_NULL_HANDLE)
			throw std::runtime_error("no suitable physical device found in the system");
	}
	/*
	 * Find suitable queue family
	 */
	{
		uint32_t q_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &q_family_count, nullptr);
		std::vector<VkQueueFamilyProperties> families(q_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &q_family_count, families.data());
		for (uint32_t i = 0; i < q_family_count; i++) {
			const auto &family = families[i];
			bool has_graphic = !!(family.queueFlags & VK_QUEUE_GRAPHICS_BIT);
			bool has_transfer = !!(family.queueFlags & VK_QUEUE_TRANSFER_BIT);
			VkBool32 has_present = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(phys_device, i, surface, &has_present);
			if (has_graphic && has_transfer && has_present) {
				queue_family_idx = i;
				break;
			}
		}
		if (queue_family_idx == UINT32_MAX)
			throw std::runtime_error("no suitable queue family found on the device");
	}
	/*
	 * Create logical device & queue
	 */
	{
		// Queue create info
		VkDeviceQueueCreateInfo queue_create_info = {};
		queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info.queueFamilyIndex = queue_family_idx;
		queue_create_info.queueCount = 1;
		float q_priority = 1.0f;
		queue_create_info.pQueuePriorities = &q_priority;
		// Device create info
		VkDeviceCreateInfo dev_create_info = {};
		dev_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		dev_create_info.pQueueCreateInfos = &queue_create_info;
		dev_create_info.queueCreateInfoCount = 1;
		VkPhysicalDeviceFeatures req_physdev_features = {};
		dev_create_info.pEnabledFeatures = &req_physdev_features;
		dev_create_info.enabledLayerCount = 0;
		requestDeviceExtensions(dev_create_info);
		if (vkCreateDevice(phys_device, &dev_create_info, allocator, &device) != VK_SUCCESS)
			throw std::runtime_error("failed to create logical device");
		vkGetDeviceQueue(device, queue_family_idx, 0, &queue);
	}
	/*
	 * Swapchain creation
	 */
	{
		bool format_found = false;
		VkSurfaceFormatKHR format;
		for (const auto &f : swapchain_info.formats) {
			if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				format = f;
				format_found = true;
				break;
			}
		}
		if (!format_found)
			throw std::runtime_error("suitable surface format not found");
		swapchain_info.image_format = format.format;

		bool mode_found = false;
		VkPresentModeKHR mode;
		for (const auto &m : swapchain_info.modes) {
			if (m == VK_PRESENT_MODE_FIFO_KHR) {
				mode = m;
				mode_found = true;
				break;
			}
		}
		if (!mode_found)
			throw std::runtime_error("suitable present mode not found");

		auto &caps = swapchain_info.caps;
		auto &extent = swapchain_info.extent;
		extent = { window_width, window_height };
		if (caps.currentExtent.width != UINT32_MAX)
			extent = caps.currentExtent;
		else {
			extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
			extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
		}

		uint32_t img_count = std::max(uint32_t(3), caps.minImageCount);
		if (caps.maxImageCount > 0)
			img_count = std::min(img_count, caps.maxImageCount);

		VkSwapchainCreateInfoKHR create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		create_info.surface = surface;
		create_info.imageFormat = swapchain_info.image_format;
		create_info.imageColorSpace = format.colorSpace;
		create_info.presentMode = mode;
		create_info.imageExtent = extent;
		create_info.minImageCount = img_count;
		create_info.imageArrayLayers = 1;
		create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		create_info.queueFamilyIndexCount = 0;
		create_info.pQueueFamilyIndices = nullptr;
		create_info.preTransform = caps.currentTransform;
		create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		create_info.clipped = VK_TRUE;
		create_info.oldSwapchain = VK_NULL_HANDLE;
		if (vkCreateSwapchainKHR(device, &create_info, allocator, &swapchain) != VK_SUCCESS)
			throw std::runtime_error("failed to create swapchain");

		img_count = 0;
		vkGetSwapchainImagesKHR(device, swapchain, &img_count, nullptr);
		Log::info("Swapchain consists of {} images", img_count);
		swapchain_info.images.resize(img_count);
		vkGetSwapchainImagesKHR(device, swapchain, &img_count, swapchain_info.images.data());
	}
	/*
	 * Swapchain image views creation
	 */
	{
		swapchain_info.image_views.resize(swapchain_info.images.size());
		VkImageViewCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		create_info.format = swapchain_info.image_format;
		create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		create_info.subresourceRange.baseMipLevel = 0;
		create_info.subresourceRange.levelCount = 1;
		create_info.subresourceRange.baseArrayLayer = 0;
		create_info.subresourceRange.layerCount = 1;
		for (size_t i = 0; i < swapchain_info.images.size(); i++) {
			create_info.image = swapchain_info.images[i];
			if (vkCreateImageView(device, &create_info, allocator, &swapchain_info.image_views[i]) != VK_SUCCESS)
				throw std::runtime_error("failed to create swapchain image view");
		}
	}
	/*
	 * Graphics pipeline layout & renderpass & pipeline itself creation
	 */
	{
		VkPipelineShaderStageCreateInfo shader_stages[2];
		VkShaderModule vert_shader = VK_NULL_HANDLE;
		{
			VkShaderModuleCreateInfo create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			create_info.codeSize = vert_spv_len;
			create_info.pCode = reinterpret_cast<const uint32_t *>(vert_spv);
			if (vkCreateShaderModule(device, &create_info, allocator, &vert_shader) != VK_SUCCESS)
				throw std::runtime_error("failed to create vertex shader");
			auto &stage_info = shader_stages[0];
			stage_info = {};
			stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
			stage_info.module = vert_shader;
			stage_info.pName = "main";
		}
		VkShaderModule frag_shader = VK_NULL_HANDLE;
		{
			VkShaderModuleCreateInfo create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			create_info.codeSize = frag_spv_len;
			create_info.pCode = reinterpret_cast<const uint32_t *>(frag_spv);
			if (vkCreateShaderModule(device, &create_info, allocator, &frag_shader) != VK_SUCCESS)
				throw std::runtime_error("failed to create fragment shader");
			auto &stage_info = shader_stages[1];
			stage_info = {};
			stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			stage_info.module = frag_shader;
			stage_info.pName = "main";
		}
		VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
		vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertex_input_info.vertexBindingDescriptionCount = 0;
		vertex_input_info.vertexAttributeDescriptionCount = 0;
		VkPipelineInputAssemblyStateCreateInfo input_assy_info = {};
		input_assy_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		input_assy_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		input_assy_info.primitiveRestartEnable = VK_FALSE;
		VkViewport viewport = {};
		viewport.x = viewport.y = 0.0f;
		viewport.width = float(window_width);
		viewport.height = float(window_height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };
		scissor.extent = swapchain_info.extent;
		VkPipelineViewportStateCreateInfo viewport_state_info = {};
		viewport_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_state_info.viewportCount = 1;
		viewport_state_info.pViewports = &viewport;
		viewport_state_info.scissorCount = 1;
		viewport_state_info.pScissors = &scissor;
		VkPipelineRasterizationStateCreateInfo raster_state_info = {};
		raster_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		raster_state_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
		raster_state_info.cullMode = VK_CULL_MODE_BACK_BIT;
		raster_state_info.polygonMode = VK_POLYGON_MODE_FILL;
		raster_state_info.lineWidth = 1.0f;
		raster_state_info.depthBiasEnable = VK_FALSE;
		raster_state_info.depthClampEnable = VK_FALSE;
		raster_state_info.rasterizerDiscardEnable = VK_FALSE;
		VkPipelineMultisampleStateCreateInfo msaa_state_info = {};
		msaa_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		msaa_state_info.pSampleMask = nullptr;
		msaa_state_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		msaa_state_info.sampleShadingEnable = VK_FALSE;
		msaa_state_info.minSampleShading = 1.0f;
		msaa_state_info.alphaToOneEnable = VK_FALSE;
		msaa_state_info.alphaToCoverageEnable = VK_FALSE;
		VkPipelineColorBlendAttachmentState color_blend_attachment = {};
		color_blend_attachment.blendEnable = VK_FALSE;
		color_blend_attachment.colorWriteMask =
		      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
		color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
		color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		VkPipelineColorBlendStateCreateInfo blend_state_info = {};
		blend_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blend_state_info.logicOpEnable = VK_FALSE;
		blend_state_info.attachmentCount = 1;
		blend_state_info.pAttachments = &color_blend_attachment;
		VkDynamicState dynamic_states[] = {
		   VK_DYNAMIC_STATE_STENCIL_REFERENCE,
		   VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
		   VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK
		};
		VkPipelineDynamicStateCreateInfo dynamic_state_info = {};
		dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_state_info.dynamicStateCount = countof(dynamic_states);
		dynamic_state_info.pDynamicStates = dynamic_states;
		{
			VkPipelineLayoutCreateInfo create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			create_info.setLayoutCount = 0;
			create_info.pSetLayouts = nullptr;
			create_info.pushConstantRangeCount = 0;
			create_info.pPushConstantRanges = nullptr;
			if (vkCreatePipelineLayout(device, &create_info, allocator, &pipeline_layout) != VK_SUCCESS)
				throw std::runtime_error("failed to create pipeline layout");
		}
		VkAttachmentDescription color_attachment = {};
		color_attachment.format = swapchain_info.image_format;
		color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		VkAttachmentReference color_attachment_ref = {};
		color_attachment_ref.attachment = 0;
		color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_attachment_ref;
		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		{
			VkRenderPassCreateInfo create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			create_info.attachmentCount = 1;
			create_info.pAttachments = &color_attachment;
			create_info.subpassCount = 1;
			create_info.pSubpasses = &subpass;
			create_info.dependencyCount = 1;
			create_info.pDependencies = &dependency;
			if (vkCreateRenderPass(device, &create_info, allocator, &renderpass) != VK_SUCCESS)
				throw std::runtime_error("failed to create renderpass");
		}
		{
			VkGraphicsPipelineCreateInfo create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			create_info.stageCount = 2;
			create_info.pStages = shader_stages;
			create_info.pVertexInputState = &vertex_input_info;
			create_info.pInputAssemblyState = &input_assy_info;
			create_info.pViewportState = &viewport_state_info;
			create_info.pRasterizationState = &raster_state_info;
			create_info.pMultisampleState = &msaa_state_info;
			create_info.pDepthStencilState = nullptr;
			create_info.pColorBlendState = &blend_state_info;
			create_info.pDynamicState = &dynamic_state_info;
			create_info.layout = pipeline_layout;
			create_info.renderPass = renderpass;
			create_info.subpass = 0;
			create_info.basePipelineHandle = VK_NULL_HANDLE;
			create_info.basePipelineIndex = -1;
			if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &create_info, allocator, &pipeline) != VK_SUCCESS)
				throw std::runtime_error("failed to create grahpics pipeline");
		}
		vkDestroyShaderModule(device, vert_shader, allocator);
		vkDestroyShaderModule(device, frag_shader, allocator);
	}
	/*
	 * Framebuffers creation
	 */
	{
		swapchain_info.framebuffers.resize(swapchain_info.images.size());
		for (size_t i = 0; i < swapchain_info.images.size(); i++) {
			VkImageView attachments[1] = {
			   swapchain_info.image_views[i]
			};
			VkFramebufferCreateInfo create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			create_info.renderPass = renderpass;
			create_info.attachmentCount = 1;
			create_info.pAttachments = attachments;
			create_info.width = swapchain_info.extent.width;
			create_info.height = swapchain_info.extent.height;
			create_info.layers = 1;
			if (vkCreateFramebuffer(device, &create_info, allocator, &swapchain_info.framebuffers[i]) != VK_SUCCESS)
				throw std::runtime_error("failed to create framebuffer");
		}
	}
	/*
	 * Command pool & command buffers creation
	 */
	{
		VkCommandPoolCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		create_info.queueFamilyIndex = queue_family_idx;
		create_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		if (vkCreateCommandPool(device, &create_info, allocator, &command_pool) != VK_SUCCESS)
			throw std::runtime_error("failed to create command pool");

		command_buffers.resize(swapchain_info.framebuffers.size());
		VkCommandBufferAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandPool = command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = uint32_t(command_buffers.size());
		if (vkAllocateCommandBuffers(device, &alloc_info, command_buffers.data()) != VK_SUCCESS)
			throw std::runtime_error("failed to allocate command buffers");
	}
	/*
	 * Sync objects creation
	 */
	{
		size_t n = swapchain_info.images.size();
		swapchain_info.image_avail_semaphores.resize(n, VK_NULL_HANDLE);
		swapchain_info.render_finish_semaphores.resize(n, VK_NULL_HANDLE);
		swapchain_info.image_free_fences.resize(n, VK_NULL_HANDLE);
		swapchain_info.items_free_fences.resize(n, VK_NULL_HANDLE);
		VkSemaphoreCreateInfo semaphore_info = {};
		semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VkFenceCreateInfo fence_info = {};
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		for (size_t i = 0; i < n; i++) {
			VkResult result = VK_SUCCESS;
			result = vkCreateSemaphore(device, &semaphore_info, allocator, &swapchain_info.image_avail_semaphores[i]);
			if (result != VK_SUCCESS) {
				Log::error("Failed to create image availability semaphore");
				throw VulkanException(result);
			}
			result = vkCreateSemaphore(device, &semaphore_info, allocator, &swapchain_info.render_finish_semaphores[i]);
			if (result != VK_SUCCESS) {
				Log::error("Failed to create render finish semaphore");
				throw VulkanException(result);
			}
			result = vkCreateFence(device, &fence_info, allocator, &swapchain_info.items_free_fences[i]);
			if (result != VK_SUCCESS) {
				Log::error("Failed to create items free fence");
				throw VulkanException(result);
			}
		}
	}
}

VulkanImpl::~VulkanImpl() {
	auto allocator = VulkanHostAllocator::callbacks();
	vkDeviceWaitIdle(device);
	for (auto &fence : swapchain_info.items_free_fences)
		vkDestroyFence(device, fence, allocator);
	for (auto &semaphore : swapchain_info.render_finish_semaphores)
		vkDestroySemaphore(device, semaphore, allocator);
	for (auto &semaphore : swapchain_info.image_avail_semaphores)
		vkDestroySemaphore(device, semaphore, allocator);
	vkFreeCommandBuffers(device, command_pool, uint32_t(command_buffers.size()), command_buffers.data());
	vkDestroyCommandPool(device, command_pool, allocator);
	for (auto &fbo : swapchain_info.framebuffers)
		vkDestroyFramebuffer(device, fbo, allocator);
	vkDestroyPipeline(device, pipeline, allocator);
	vkDestroyRenderPass(device, renderpass, allocator);
	vkDestroyPipelineLayout(device, pipeline_layout, allocator);
	for (auto &img_view : swapchain_info.image_views)
		vkDestroyImageView(device, img_view, allocator);
	vkDestroySwapchainKHR(device, swapchain, allocator);
	vkDestroyDevice(device, allocator);
	vkDestroySurfaceKHR(instance, surface, allocator);
	vkDestroyInstance(instance, allocator);
}

void VulkanImpl::beginFrame() {
	VkResult result;
	result = vkWaitForFences(device, 1, &swapchain_info.items_free_fences[current_items_idx], VK_TRUE, UINT64_MAX);
	if (result != VK_SUCCESS) {
		Log::error("Failure on waiting for fence");
		throw VulkanException(result);
	}

	result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
	                               swapchain_info.image_avail_semaphores[current_items_idx],
	                               VK_NULL_HANDLE, &current_image_idx);
	if (result != VK_SUCCESS) {
		Log::error("Failed to acquire image from swapchain");
		throw VulkanException(result);
	}

	result = vkResetCommandBuffer(command_buffers[current_items_idx], 0);
	if (result != VK_SUCCESS) {
		Log::error("Failed to reset command buffer");
		throw VulkanException(result);
	}
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	result = vkBeginCommandBuffer(command_buffers[current_items_idx], &begin_info);
	if (result != VK_SUCCESS) {
		Log::error("Failed to begin recording command buffer");
		throw VulkanException(result);
	}

	VkRenderPassBeginInfo renderpass_begin_info = {};
	renderpass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderpass_begin_info.renderPass = renderpass;
	renderpass_begin_info.framebuffer = swapchain_info.framebuffers[current_image_idx];
	renderpass_begin_info.renderArea.offset = { 0, 0 };
	renderpass_begin_info.renderArea.extent = swapchain_info.extent;
	VkClearValue clear_color;
	clear_color.color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
	renderpass_begin_info.clearValueCount = 1;
	renderpass_begin_info.pClearValues = &clear_color;
	vkCmdBeginRenderPass(command_buffers[current_items_idx], &renderpass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanImpl::endFrame() {
	vkCmdEndRenderPass(command_buffers[current_items_idx]);
	VkResult result = vkEndCommandBuffer(command_buffers[current_items_idx]);
	if (result != VK_SUCCESS) {
		Log::error("Failure when recording command buffer");
		throw VulkanException(result);
	}

	VkPipelineStageFlags wait_stages[1] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &swapchain_info.image_avail_semaphores[current_items_idx];
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffers[current_items_idx];
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &swapchain_info.render_finish_semaphores[current_items_idx];

	if (swapchain_info.image_free_fences[current_image_idx] != VK_NULL_HANDLE)
		vkWaitForFences(device, 1, &swapchain_info.image_free_fences[current_image_idx], VK_TRUE, UINT64_MAX);
	swapchain_info.image_free_fences[current_image_idx] = swapchain_info.items_free_fences[current_items_idx];
	vkResetFences(device, 1, &swapchain_info.items_free_fences[current_items_idx]);

	if (vkQueueSubmit(queue, 1, &submit_info, swapchain_info.items_free_fences[current_items_idx]) != VK_SUCCESS)
		throw std::runtime_error("failed to submit command buffer");

	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &swapchain_info.render_finish_semaphores[current_items_idx];
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &swapchain;
	present_info.pImageIndices = &current_image_idx;
	present_info.pResults = nullptr;
	vkQueuePresentKHR(queue, &present_info);

	current_items_idx = (current_items_idx + 1) % uint32_t(swapchain_info.images.size());
}

void VulkanImpl::requestInstanceLayers(VkInstanceCreateInfo &create_info) {
	if (!BuildConfig::kUseVulkanDebugging) {
		create_info.enabledLayerCount = 0;
		return;
	}

	static const char *need_layers[] = {
	   "VK_LAYER_LUNARG_standard_validation",
	   "VK_LAYER_KHRONOS_validation",
	   "VK_LAYER_MESA_overlay",
	};
	constexpr size_t need_layers_cnt = countof(need_layers);
	bool need_layer_found[need_layers_cnt];
	for (size_t i = 0; i < need_layers_cnt; i++)
		need_layer_found[i] = false;

	uint32_t layer_count;
	vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
	std::vector<VkLayerProperties> layer_props(layer_count);
	vkEnumerateInstanceLayerProperties(&layer_count, layer_props.data());
	Log::info("There are {} Vulkan instance layers available:", layer_count);
	for (const auto &prop : layer_props) {
		char buf[256];
		uint32_t sv = prop.specVersion;
		uint32_t iv = prop.implementationVersion;
		snprintf(buf, countof(buf), "spec version %u.%u.%u, impl version %u",
		         VK_VERSION_MAJOR(sv), VK_VERSION_MINOR(sv), VK_VERSION_PATCH(sv), iv);
		Log::info("Layer {}, {}", prop.layerName, buf);
		for (size_t i = 0; i < need_layers_cnt; i++) {
			if (!strcmp(need_layers[i], prop.layerName)) {
				need_layer_found[i] = true;
				break;
			}
		}
	}

	for (size_t i = 0; i < need_layers_cnt; i++) {
		if (!need_layer_found[i]) {
			Log::error("Requested Vulkan instance layer {} is not available!", need_layers[i]);
			throw std::runtime_error("requested Vulkan instance layer is not available");
		}
	}
	create_info.enabledLayerCount = uint32_t(need_layers_cnt);
	create_info.ppEnabledLayerNames = need_layers;
}

void VulkanImpl::requestDeviceExtensions(VkDeviceCreateInfo &create_info) {
	static const char *need_exts[] = {
	   VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};
	constexpr size_t need_exts_cnt = countof(need_exts);
	bool need_ext_found[need_exts_cnt];
	for (size_t i = 0; i < need_exts_cnt; i++)
		need_ext_found[i] = false;

	uint32_t ext_count;
	vkEnumerateDeviceExtensionProperties(phys_device, nullptr, &ext_count, nullptr);
	std::vector<VkExtensionProperties> ext_props(ext_count);
	vkEnumerateDeviceExtensionProperties(phys_device, nullptr, &ext_count, ext_props.data());
	for (const auto &prop : ext_props) {
		for (size_t i = 0; i < need_exts_cnt; i++) {
			if (!strcmp(need_exts[i], prop.extensionName)) {
				need_ext_found[i] = true;
				break;
			}
		}
	}

	for (size_t i = 0; i < need_exts_cnt; i++) {
		if (!need_ext_found[i]) {
			Log::error("Requested device extension {} is not available!", need_exts[i]);
			throw std::runtime_error("requested device extension is not available");
		}
	}
	create_info.enabledExtensionCount = uint32_t(need_exts_cnt);
	create_info.ppEnabledExtensionNames = need_exts;
}

bool VulkanImpl::isDeviceSuitable(VkPhysicalDevice device) {
	VkPhysicalDeviceProperties props;
	VkPhysicalDeviceFeatures features;
	vkGetPhysicalDeviceProperties(device, &props);
	vkGetPhysicalDeviceFeatures(device, &features);
	Log::info("Found physical device {}", props.deviceName);

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &swapchain_info.caps);

	uint32_t format_cnt;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_cnt, nullptr);
	swapchain_info.formats.resize(format_cnt);
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_cnt, swapchain_info.formats.data());

	uint32_t present_mode_cnt;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_cnt, nullptr);
	swapchain_info.modes.resize(present_mode_cnt);
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_cnt, swapchain_info.modes.data());

	if (format_cnt == 0 || present_mode_cnt == 0)
		return false;

	//TODO: better solution?
	if (voxen::BuildConfig::kUseIntegratedGpu) {
		if (props.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
			Log::info("GPU {} is what we are looking for", props.deviceName);
			return true;
		}
	} else {
		if (props.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			Log::info("GPU {} is what we are looking for", props.deviceName);
			return true;
		}
	}
	return false;
}

}
