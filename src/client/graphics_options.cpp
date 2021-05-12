#include <voxen/client/graphics_options.hpp>

namespace voxen::client
{

void GraphicsOptions::scanOptionsSupport()
{

}

std::vector<GraphicsOptions::FrameSize> GraphicsOptions::availableFrameSizes() const
{
	// TODO (Svenny): request them from windowing system?
	return {
		// 4:3
		{ 640, 480 },
		{ 800, 600 },
		{ 1024, 768 },
		{ 1600, 1200 },
		// 16:9
		{ 1280, 720 },
		{ 1366, 768 },
		{ 1600, 900 },
		{ 1920, 1080 },
		{ 2560, 1440 },
		{ 3840, 2160 },
		// 16:10
		{ 1280, 800 },
		{ 1440, 900 },
		{ 1680, 1050 },
		{ 1920, 1200 },
		{ 2560, 1600 }
	};
}

std::pair<float, float> GraphicsOptions::sceneResolutionScaleRange() const
{
	// TODO (Svenny): check against framebuffer/image size limits,
	return { 0.125f, 8.0f };
}

bool GraphicsOptions::isSceneRescaleMethodSupported(RescaleMethod method) const
{
	// TODO (Svenny): implement other methods
	return method == RescaleMethod::Linear;
}

bool GraphicsOptions::aaMethodSupported(AaMethod method) const
{
	// TODO (Svenny): implement AA
	return method == AaMethod::None;
}

bool GraphicsOptions::afValueSuppored(int32_t value) const
{
	// TODO (Svenny): check for AF support
	return value == 1;
}

bool GraphicsOptions::isExtendedDepthSupported() const
{
	// TODO (Svenny): check for D32 format & unrestricted depth range support
	return false;
}

bool GraphicsOptions::isCsmSupported() const
{
	// TODO (Svenny): check for D16, multiview and other shadow-related stuff support
	return false;
}

std::pair<int32_t, int32_t> GraphicsOptions::csmDimensionRange() const
{
	// TODO (Svenny): check for actual limits
	return { 256, 16384 };
}

std::pair<int32_t, int32_t> GraphicsOptions::csmNumLayersRange() const
{
	// TODO (Svenny): check for actual limits
	return { 1, 32 };
}

}
