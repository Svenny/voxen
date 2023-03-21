#pragma once

namespace voxen
{

class BuildConfig {
public:
#if VOXEN_DEBUG_BUILD == 1
	constexpr static bool kIsDebugBuild = true;
	constexpr static bool kIsReleaseBuild = false;
#else
	constexpr static bool kIsDebugBuild = false;
	constexpr static bool kIsReleaseBuild = true;
#endif
	constexpr static bool kUseVulkanDebugging = @USE_VULKAN_DEBUGGING@;
	constexpr static bool kUseIntegratedGpu = @USE_INTEGRATED_GPU@;
	constexpr static bool kIsDeployBuild = @DEPLOY_BUILD@;
};

}
