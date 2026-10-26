#pragma once

#include <voxen/gfx/gfx_fwd.hpp>
#include <voxen/gfx/vk/vk_include.hpp>

#include <string>

namespace voxen::gfx::vk
{

// Wrapper of `VkPipelineCache` persisting its contents in a user data file
class PipelineCache {
public:
	// Initial cache contents are loaded from `path` (relative to the user
	// data directory) if it exists. Empty path disables persistence.
	explicit PipelineCache(Device &dev, std::string path);
	PipelineCache(PipelineCache &&) = delete;
	PipelineCache(const PipelineCache &) = delete;
	PipelineCache &operator=(PipelineCache &&) = delete;
	PipelineCache &operator=(const PipelineCache &) = delete;
	// Calls `dump()` before destroying the cache
	~PipelineCache() noexcept;

	// Save cache contents to the file, returns false on failure.
	// Nothrow because it is automatically called in the destructor.
	bool dump() noexcept;

	VkPipelineCache handle() const noexcept { return m_handle; }

private:
	Device &m_dev;
	VkPipelineCache m_handle = VK_NULL_HANDLE;
	std::string m_save_path;
};

} // namespace voxen::gfx::vk
