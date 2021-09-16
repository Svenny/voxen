#include <voxen/client/vulkan/descriptor_set_layout.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>
#include <voxen/util/log.hpp>

namespace voxen::client::vulkan
{

WrappedVkDescriptorSetLayout::WrappedVkDescriptorSetLayout(const VkDescriptorSetLayoutCreateInfo &info)
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	VkResult result = backend.vkCreateDescriptorSetLayout(device, &info, HostAllocator::callbacks(), &m_handle);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkCreateDescriptorSetLayout");
	}
}

WrappedVkDescriptorSetLayout::WrappedVkDescriptorSetLayout(WrappedVkDescriptorSetLayout &&other) noexcept
{
	m_handle = std::exchange(other.m_handle, static_cast<VkDescriptorSetLayout>(VK_NULL_HANDLE));
}

WrappedVkDescriptorSetLayout &WrappedVkDescriptorSetLayout::operator = (WrappedVkDescriptorSetLayout &&other) noexcept
{
	std::swap(m_handle, other.m_handle);
	return *this;
}

WrappedVkDescriptorSetLayout::~WrappedVkDescriptorSetLayout() noexcept
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	backend.vkDestroyDescriptorSetLayout(device, m_handle, HostAllocator::callbacks());
}

DescriptorSetLayoutCollection::DescriptorSetLayoutCollection()
	: m_main_scene_layout(createMainSceneLayout()),
	m_terrain_frustum_cull_layout(createTerrainFrustumCullLayout())
{
	Log::debug("DescriptorSetLayoutCollection created successfully");
}

WrappedVkDescriptorSetLayout DescriptorSetLayoutCollection::createMainSceneLayout()
{
	VkDescriptorSetLayoutBinding bindings[1];
	bindings[0] = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_ALL,
		.pImmutableSamplers = nullptr
	};

	return WrappedVkDescriptorSetLayout(VkDescriptorSetLayoutCreateInfo {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.bindingCount = 1,
		.pBindings = bindings,
	});
}

WrappedVkDescriptorSetLayout DescriptorSetLayoutCollection::createTerrainFrustumCullLayout()
{
	VkDescriptorSetLayoutBinding bindings[3];
	bindings[0] = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr
	};
	bindings[1] = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr
	};
	bindings[2] = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr
	};

	return WrappedVkDescriptorSetLayout(VkDescriptorSetLayoutCreateInfo {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.bindingCount = 3,
		.pBindings = bindings,
	});
}

}
