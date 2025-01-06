#include <voxen/gfx/vk/vk_error.hpp>

#include <voxen/gfx/vk/vk_utils.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/log.hpp>

#include <fmt/format.h>

#include <cassert>

namespace
{

struct VulkanErrorCategory final : std::error_category {
	const char *name() const noexcept override { return "vulkan"; }

	std::string message(int code) const override
	{
		return std::string(voxen::gfx::vk::VulkanUtils::getVkResultString(VkResult(code)));
	}

	std::error_condition default_error_condition(int /*code*/) const noexcept override
	{
		// Map all Vulkan error codes to a generic `GfxFailure`.
		// Not ideal but being more fine-grained here is hard.
		return voxen::VoxenErrc::GfxFailure;
	}
};

const VulkanErrorCategory g_category;

std::string formatMessage(VkResult result, const char *api)
{
	return fmt::format("'{}' failed: {}", api, voxen::gfx::vk::VulkanUtils::getVkResultString(result));
}

} // anonymous namespace

namespace std
{

error_code make_error_code(VkResult result) noexcept
{
	return { static_cast<int>(result), g_category };
}

} // namespace std

namespace voxen::gfx::vk
{

VulkanException::VulkanException(VkResult result, const char *api, extras::source_location loc)
	: Exception(formatMessage(result, api), std::make_error_code(result).default_error_condition(), loc)
{}

// Not defining inline to satisfy -Wweak-vtables
VulkanException::~VulkanException() noexcept = default;

} // namespace voxen::gfx::vk
