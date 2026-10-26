#include <voxen/gfx/vk/vk_pipeline_cache.hpp>

#include <voxen/common/filemanager.hpp>
#include <voxen/gfx/vk/vk_device.hpp>
#include <voxen/gfx/vk/vk_error.hpp>
#include <voxen/gfx/vk/vk_utils.hpp>
#include <voxen/util/log.hpp>

#include <extras/defer.hpp>

#include <cstddef>
#include <cstdlib>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace voxen::gfx::vk
{

namespace
{

constexpr size_t MAX_PIPELINE_CACHE_SIZE = 1 << 25; // 32 MB

} // namespace

PipelineCache::PipelineCache(Device &dev, std::string path) : m_dev(dev), m_save_path(std::move(path))
{
	Log::debug("Creating PipelineCache");

	std::optional<std::vector<std::byte>> cache_data;
	if (!m_save_path.empty()) {
		Log::debug("Trying to preinitialize pipeline cache from `{}`", m_save_path);
		cache_data = FileManager::readUserFile(m_save_path);
	}

	VkPipelineCacheCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	if (cache_data.has_value()) {
		info.initialDataSize = cache_data->size();
		info.pInitialData = cache_data->data();
	}

	VkResult result = m_dev.dt().vkCreatePipelineCache(m_dev.handle(), &info, nullptr, &m_handle);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkCreatePipelineCache");
	}
	Log::debug("PipelineCache created successfully");
}

PipelineCache::~PipelineCache() noexcept
{
	Log::debug("Destroying PipelineCache");
	dump();
	m_dev.dt().vkDestroyPipelineCache(m_dev.handle(), m_handle, nullptr);
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

	// On `VK_INCOMPLETE` the written part is still valid cache data
	size_t buffer_size = MAX_PIPELINE_CACHE_SIZE;
	VkResult result = m_dev.dt().vkGetPipelineCacheData(m_dev.handle(), m_handle, &buffer_size, buffer);
	if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
		Log::warn("Vulkan API error when saving pipeline cache: {}", VulkanUtils::getVkResultString(result));
		return false;
	}

	Log::debug("Saving {} bytes of pipeline cache data to {}", buffer_size, m_save_path);
	return FileManager::writeUserFile(m_save_path, std::span(static_cast<std::byte *>(buffer), buffer_size));
}

} // namespace voxen::gfx::vk
