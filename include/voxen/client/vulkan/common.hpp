#pragma once

#include <voxen/util/exception.hpp>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <atomic>
#include <string>
#include <string_view>

namespace voxen::client::vulkan
{

// Some stateless utility functions
class VulkanUtils final {
public:
	VulkanUtils() = delete;

	static std::string_view getVkResultString(VkResult result) noexcept;
	static std::string_view getVkFormatString(VkFormat format) noexcept;

	// Returns minimal integer multiple of `alignment` not less than `size`. `alignment` must be a power of two.
	static uint32_t alignUp(uint32_t size, uint32_t alignment) noexcept;
	static uint64_t alignUp(uint64_t size, uint64_t alignment) noexcept;
	// Multiplies `size` by `numerator` and then divides by `denomenator` with rounding up.
	static uint64_t calcFraction(uint64_t size, uint64_t numerator, uint64_t denomenator) noexcept;
};

class VulkanException : public Exception {
public:
	explicit VulkanException(VkResult result, const char *api = nullptr, extras::source_location loc =
		extras::source_location::current()) noexcept;
	virtual ~VulkanException() = default;

	virtual const char *what() const noexcept override;
	VkResult result() const noexcept { return m_result; }

protected:
	std::string m_message;
	VkResult m_result;
	bool m_exception_occured = false;
};

class HostAllocator {
public:
	HostAllocator() noexcept;
	HostAllocator(HostAllocator &&) = delete;
	HostAllocator(const HostAllocator &) = delete;
	HostAllocator &operator = (HostAllocator &&) = delete;
	HostAllocator &operator = (const HostAllocator &) = delete;
	~HostAllocator() noexcept;

	static HostAllocator &instance() noexcept { return g_instance; }
	static const VkAllocationCallbacks *callbacks() noexcept;
	static size_t allocated() noexcept { return g_instance.m_allocated.load(); }

private:
	static HostAllocator g_instance;

	VkAllocationCallbacks m_callbacks;
	std::atomic_size_t m_allocated;
};

}
