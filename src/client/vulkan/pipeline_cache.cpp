#include <voxen/client/vulkan/pipeline_cache.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/gfx/vk/vk_device.hpp>

#include <voxen/common/filemanager.hpp>
#include <voxen/util/log.hpp>

#include <extras/defer.hpp>

#include <cstdlib>

namespace voxen::client::vulkan
{

inline constexpr size_t MAX_PIPELINE_CACHE_SIZE = 1 << 25; // 32 MB

PipelineCache::PipelineCache(const char *path) : m_save_path(path)
{
	Log::debug("Creating PipelineCache");
	if (path) {
		Log::debug("Trying to preinitialize pipeline cache from `{}`", path);
	}
	auto cache_data = FileManager::readUserFile(path);

	VkPipelineCacheCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	if (cache_data.has_value()) {
		auto &data = cache_data.value();
		info.initialDataSize = data.size();
		info.pInitialData = data.data();
	}

	auto &backend = Backend::backend();
	VkDevice device = backend.device().handle();
	VkResult result = backend.vkCreatePipelineCache(device, &info, HostAllocator::callbacks(), &m_cache);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkCreatePipelineCache");
	}
	Log::debug("PipelineCache created successfully");
}

bool PipelineCache::dump() noexcept
{
	if (m_save_path.empty()) {
		return false;
	}

	// malloc is used for noexcept behavior
	void *buffer = malloc(MAX_PIPELINE_CACHE_SIZE);
	if (!buffer) {
		Log::warn("Out of memory when saving pipeline cache");
		return false;
	}
	defer { free(buffer); };

	size_t buffer_size = MAX_PIPELINE_CACHE_SIZE;
	auto &backend = Backend::backend();
	VkDevice device = backend.device().handle();
	VkResult result = backend.vkGetPipelineCacheData(device, m_cache, &buffer_size, buffer);
	if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
		Log::warn("Vulkan API error when saving pipeline cache: {}", VulkanUtils::getVkResultString(result));
		return false;
	} else if (result == VK_INCOMPLETE) {
		size_t full_size;
		result = backend.vkGetPipelineCacheData(device, m_cache, &full_size, nullptr);
	}

	Log::debug("Saving {} bytes of pipeline cache data to {}", buffer_size, m_save_path);
	return FileManager::writeUserFile(m_save_path, buffer, buffer_size);
}

PipelineCache::~PipelineCache() noexcept
{
	Log::debug("Destroying PipelineCache");
	dump();

	auto &backend = Backend::backend();
	VkDevice device = backend.device().handle();
	backend.vkDestroyPipelineCache(device, m_cache, HostAllocator::callbacks());
}

} // namespace voxen::client::vulkan
