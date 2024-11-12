#pragma once

#include <voxen/gfx/vk/vk_include.hpp>

#include <string>

namespace voxen::client::vulkan
{

class PipelineCache {
public:
	explicit PipelineCache(const char *path = nullptr);
	PipelineCache(PipelineCache &&) = delete;
	PipelineCache(const PipelineCache &) = delete;
	PipelineCache &operator=(PipelineCache &&) = delete;
	PipelineCache &operator=(const PipelineCache &) = delete;
	~PipelineCache() noexcept;

	// noexcept style is used because this function is automatically called in destructor
	bool dump() noexcept;

	operator VkPipelineCache() const noexcept { return m_cache; }

private:
	VkPipelineCache m_cache = VK_NULL_HANDLE;
	std::string m_save_path;
};

} // namespace voxen::client::vulkan
