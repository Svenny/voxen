#include <voxen/gfx/vk/vk_error.hpp>

#include <voxen/gfx/vk/vk_utils.hpp>
#include <voxen/util/log.hpp>

#include <fmt/format.h>

#include <cassert>

namespace
{

struct VulkanErrorCategory : std::error_category {
	const char *name() const noexcept override { return "Vulkan error"; }

	std::string message(int code) const override
	{
		return std::string(voxen::gfx::vk::VulkanUtils::getVkResultString(VkResult(code)));
	}
};

const VulkanErrorCategory g_category;

} // anonymous namespace

namespace std
{

error_condition make_error_condition(VkResult result) noexcept
{
	return { static_cast<int>(result), g_category };
}

} // namespace std

namespace voxen::gfx::vk
{

VulkanException::VulkanException(VkResult result, std::string_view api, extras::source_location loc)
	: Exception(fmt::format("call to '{}' failed", api), std::make_error_condition(result), loc)
{
	assert(api.length() > 0);
	Log::error("{} failed with error code {}", api, VulkanUtils::getVkResultString(result), loc);
}

// Not defining inline to satisfy -Wweak-vtables
VulkanException::~VulkanException() noexcept = default;

VkResult VulkanException::result() const noexcept
{
	// We know the error category can only be vulkan one
	return static_cast<VkResult>(error().value());
}

} // namespace voxen::gfx::vk
