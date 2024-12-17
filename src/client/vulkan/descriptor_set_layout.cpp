#include <voxen/client/vulkan/descriptor_set_layout.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/gfx/vk/vk_device.hpp>
#include <voxen/gfx/vk/vk_error.hpp>
#include <voxen/util/log.hpp>

namespace voxen::client::vulkan
{

WrappedVkDescriptorSetLayout::WrappedVkDescriptorSetLayout(const VkDescriptorSetLayoutCreateInfo &info)
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device().handle();
	VkResult result = backend.vkCreateDescriptorSetLayout(device, &info, nullptr, &m_handle);
	if (result != VK_SUCCESS) {
		throw gfx::vk::VulkanException(result, "vkCreateDescriptorSetLayout");
	}
}

WrappedVkDescriptorSetLayout::WrappedVkDescriptorSetLayout(WrappedVkDescriptorSetLayout &&other) noexcept
{
	m_handle = std::exchange(other.m_handle, static_cast<VkDescriptorSetLayout>(VK_NULL_HANDLE));
}

WrappedVkDescriptorSetLayout &WrappedVkDescriptorSetLayout::operator=(WrappedVkDescriptorSetLayout &&other) noexcept
{
	std::swap(m_handle, other.m_handle);
	return *this;
}

WrappedVkDescriptorSetLayout::~WrappedVkDescriptorSetLayout() noexcept
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device().handle();
	backend.vkDestroyDescriptorSetLayout(device, m_handle, nullptr);
}

DescriptorSetLayoutCollection::DescriptorSetLayoutCollection()
	: m_main_scene_layout(createMainSceneLayout())
	, m_terrain_frustum_cull_layout(createTerrainFrustumCullLayout())
	, m_ui_font_layout(createUiFontLayout())
{
	Log::debug("DescriptorSetLayoutCollection created successfully");
}

void DescriptorSetLayoutCollection::appendDescriptorConsumption(const VkDescriptorSetLayoutCreateInfo &info)
{
	for (uint32_t i = 0; i < info.bindingCount; i++) {
		VkDescriptorType type = info.pBindings[i].descriptorType;
		uint32_t count = info.pBindings[i].descriptorCount;
		m_descriptor_consumption[type] += count;
	}
}

WrappedVkDescriptorSetLayout DescriptorSetLayoutCollection::createMainSceneLayout()
{
	VkDescriptorSetLayoutBinding bindings[1];
	bindings[0] = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_ALL,
		.pImmutableSamplers = nullptr,
	};

	const VkDescriptorSetLayoutCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.bindingCount = 1,
		.pBindings = bindings,
	};

	appendDescriptorConsumption(info);
	return WrappedVkDescriptorSetLayout(info);
}

WrappedVkDescriptorSetLayout DescriptorSetLayoutCollection::createTerrainFrustumCullLayout()
{
	VkDescriptorSetLayoutBinding bindings[3];
	bindings[0] = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr,
	};
	bindings[1] = {
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr,
	};
	bindings[2] = {
		.binding = 2,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr,
	};

	const VkDescriptorSetLayoutCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.bindingCount = std::size(bindings),
		.pBindings = bindings,
	};

	appendDescriptorConsumption(info);
	return WrappedVkDescriptorSetLayout(info);
}

WrappedVkDescriptorSetLayout DescriptorSetLayoutCollection::createUiFontLayout()
{
	VkDescriptorSetLayoutBinding bindings[2];
	bindings[0] = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	};
	bindings[1] = {
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.pImmutableSamplers = nullptr,
	};

	const VkDescriptorSetLayoutCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
		.bindingCount = std::size(bindings),
		.pBindings = bindings,
	};

	appendDescriptorConsumption(info);
	return WrappedVkDescriptorSetLayout(info);
}

} // namespace voxen::client::vulkan
