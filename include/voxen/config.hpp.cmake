#pragma once

namespace voxen
{

class BuildConfig {
public:
	constexpr static bool kUseVulkanDebugging = @USE_VULKAN_DEBUGGING@;
};

}
