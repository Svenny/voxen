#pragma once

namespace voxen
{

class BuildConfig {
public:
	constexpr static int kVersionMajor = @VOXEN_VERSION_MAJOR@;
	constexpr static int kVersionMinor = @VOXEN_VERSION_MINOR@;
	constexpr static int kVersionPatch = @VOXEN_VERSION_PATCH@;
	constexpr static const char kVersionSuffix[] = "@VOXEN_VERSION_SUFFIX@";
	constexpr static const char kVersionString[] = "@VOXEN_VERSION_STRING@";

#ifdef _NDEBUG
	constexpr static bool kIsDebugBuild = false;
	constexpr static bool kIsReleaseBuild = true;
#else
	constexpr static bool kIsDebugBuild = true;
	constexpr static bool kIsReleaseBuild = false;
#endif
	constexpr static bool kUseVulkanDebugging = @USE_VULKAN_DEBUGGING@;
	constexpr static bool kUseIntegratedGpu = @USE_INTEGRATED_GPU@;
	constexpr static bool kIsDeployBuild = @DEPLOY_BUILD@;
};

}
