#pragma once

#include <voxen/util/exception.hpp>

#include <vulkan/vulkan.h>

#include <atomic>
#include <string>

namespace voxen::client
{

const char *getVkResultString(VkResult result) noexcept;
const char *getVkResultDescription(VkResult result) noexcept;

class VulkanException : public Exception {
public:
	explicit VulkanException(VkResult result, const std::experimental::source_location &loc =
	      std::experimental::source_location::current());
	virtual ~VulkanException() override = default;

	virtual const char *what() const noexcept override { return m_message.c_str(); }
	VkResult result() const noexcept { return m_result; }
protected:
	VkResult m_result;
	std::string m_message;
};

class VulkanHostAllocator {
public:
	VulkanHostAllocator();
	~VulkanHostAllocator();

	static VulkanHostAllocator &instance() noexcept { return g_instance; }
	static const VkAllocationCallbacks *callbacks() noexcept { return &g_instance.m_callbacks; }
	static size_t allocated() noexcept { return g_instance.m_allocated.load(); }
private:
	static VulkanHostAllocator g_instance;

	VkAllocationCallbacks m_callbacks;
	std::atomic_size_t m_allocated;
};

}
