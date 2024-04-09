#include <voxen/client/vulkan/descriptor_manager.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/descriptor_set_layout.hpp>
#include <voxen/client/vulkan/device.hpp>

#include <extras/dyn_array.hpp>

namespace voxen::client::vulkan
{

WrappedVkDescriptorPool::WrappedVkDescriptorPool(const VkDescriptorPoolCreateInfo &info)
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	VkResult result = backend.vkCreateDescriptorPool(device, &info, HostAllocator::callbacks(), &m_handle);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkCreateDescriptorPool");
	}
}

WrappedVkDescriptorPool::WrappedVkDescriptorPool(WrappedVkDescriptorPool &&other) noexcept
{
	m_handle = std::exchange(other.m_handle, static_cast<VkDescriptorPool>(VK_NULL_HANDLE));
}

WrappedVkDescriptorPool &WrappedVkDescriptorPool::operator=(WrappedVkDescriptorPool &&other) noexcept
{
	std::swap(m_handle, other.m_handle);
	return *this;
}

WrappedVkDescriptorPool::~WrappedVkDescriptorPool() noexcept
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	backend.vkDestroyDescriptorPool(device, m_handle, HostAllocator::callbacks());
}

DescriptorManager::DescriptorManager()
{
	auto &backend = Backend::backend();
	auto &layout_collection = backend.descriptorSetLayoutCollection();
	const auto &consumption = layout_collection.totalDescriptorConsumption();

	extras::dyn_array<VkDescriptorPoolSize> sizes(consumption.size());
	auto iter = sizes.begin();
	for (const auto [type, count] : consumption) {
		iter->type = type;
		iter->descriptorCount = count * Config::NUM_CPU_PENDING_FRAMES;
		++iter;
	}

	const VkDescriptorPoolCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		// TODO: this value needs to be updated when a new set layout is added
		.maxSets = 2 * Config::NUM_CPU_PENDING_FRAMES,
		.poolSizeCount = static_cast<uint32_t>(sizes.size()),
		.pPoolSizes = sizes.data(),
	};
	m_main_pool = WrappedVkDescriptorPool(info);

	VkDescriptorSetLayout set_layouts[Config::NUM_CPU_PENDING_FRAMES];
	VkDescriptorSetAllocateInfo alloc_info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = nullptr,
		.descriptorPool = m_main_pool,
		.descriptorSetCount = Config::NUM_CPU_PENDING_FRAMES,
		.pSetLayouts = set_layouts,
	};

	VkDevice device = backend.device();
	VkResult result;

	std::fill(std::begin(set_layouts), std::end(set_layouts), layout_collection.mainSceneLayout());
	result = backend.vkAllocateDescriptorSets(device, &alloc_info, &m_main_scene_set[0]);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkAllocateDescriptorSets");
	}

	std::fill(std::begin(set_layouts), std::end(set_layouts), layout_collection.terrainFrustumCullLayout());
	result = backend.vkAllocateDescriptorSets(device, &alloc_info, &m_terrain_frustum_cull_set[0]);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkAllocateDescriptorSets");
	}
}

} // namespace voxen::client::vulkan
