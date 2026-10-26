#include <voxen/gfx/vk/vk_descriptor_set_layout.hpp>

#include <voxen/gfx/vk/vk_device.hpp>
#include <voxen/gfx/vk/vk_error.hpp>
#include <voxen/log.hpp>

#include <extras/defer.hpp>

#include <cstdint>
#include <span>

namespace voxen::gfx::vk
{

namespace
{

VkDescriptorSetLayout createLayout(Device &dev, VkDescriptorSetLayoutCreateFlags flags,
	std::span<const VkDescriptorSetLayoutBinding> bindings)
{
	const VkDescriptorSetLayoutCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = flags,
		.bindingCount = uint32_t(bindings.size()),
		.pBindings = bindings.data(),
	};

	VkDescriptorSetLayout handle = VK_NULL_HANDLE;
	VkResult result = dev.dt().vkCreateDescriptorSetLayout(dev.handle(), &info, nullptr, &handle);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkCreateDescriptorSetLayout");
	}

	return handle;
}

constexpr VkDescriptorSetLayoutBinding makeBinding(uint32_t binding, VkDescriptorType type,
	VkShaderStageFlags stages) noexcept
{
	return VkDescriptorSetLayoutBinding {
		.binding = binding,
		.descriptorType = type,
		.descriptorCount = 1,
		.stageFlags = stages,
		.pImmutableSamplers = nullptr,
	};
}

constexpr VkDescriptorSetLayoutCreateFlags PUSH_DESCRIPTOR_FLAG
	= VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;

constexpr VkDescriptorSetLayoutBinding MAIN_SCENE_BINDINGS[] = {
	makeBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL),
};

constexpr VkDescriptorSetLayoutBinding LAND_FRUSTUM_CULL_BINDINGS[] = {
	makeBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT),
	makeBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT),
	makeBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT),
};

constexpr VkDescriptorSetLayoutBinding LAND_CHUNK_MESH_BINDINGS[] = {
	makeBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT),
	makeBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT),
};

constexpr VkDescriptorSetLayoutBinding UI_FONT_BINDINGS[] = {
	makeBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
	makeBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT),
};

constexpr VkDescriptorSetLayoutBinding UI_BASIC_BINDINGS[] = {
	makeBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT),
	makeBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT),
};

} // namespace

DescriptorSetLayoutCollection::DescriptorSetLayoutCollection(Device &dev) : m_dev(dev)
{
	defer_fail { destroyLayouts(); };

	m_main_scene_layout = createLayout(dev, 0, MAIN_SCENE_BINDINGS);
	m_land_frustum_cull_layout = createLayout(dev, PUSH_DESCRIPTOR_FLAG, LAND_FRUSTUM_CULL_BINDINGS);
	m_land_chunk_mesh_layout = createLayout(dev, PUSH_DESCRIPTOR_FLAG, LAND_CHUNK_MESH_BINDINGS);
	m_ui_font_layout = createLayout(dev, PUSH_DESCRIPTOR_FLAG, UI_FONT_BINDINGS);
	m_ui_basic_layout = createLayout(dev, PUSH_DESCRIPTOR_FLAG, UI_BASIC_BINDINGS);

	Log::debug("DescriptorSetLayoutCollection created successfully");
}

DescriptorSetLayoutCollection::~DescriptorSetLayoutCollection() noexcept
{
	destroyLayouts();
}

void DescriptorSetLayoutCollection::destroyLayouts() noexcept
{
	// Destroying null handles is allowed, no need to check
	VkDevice device = m_dev.handle();
	m_dev.dt().vkDestroyDescriptorSetLayout(device, m_ui_basic_layout, nullptr);
	m_dev.dt().vkDestroyDescriptorSetLayout(device, m_ui_font_layout, nullptr);
	m_dev.dt().vkDestroyDescriptorSetLayout(device, m_land_chunk_mesh_layout, nullptr);
	m_dev.dt().vkDestroyDescriptorSetLayout(device, m_land_frustum_cull_layout, nullptr);
	m_dev.dt().vkDestroyDescriptorSetLayout(device, m_main_scene_layout, nullptr);
}

} // namespace voxen::gfx::vk
