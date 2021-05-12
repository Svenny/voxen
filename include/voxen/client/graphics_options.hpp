#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace voxen::client
{

class GraphicsOptions {
public:
	enum class Option : uint32_t {
		FrameSize,
		SceneResolutionScale,
		SceneRescaleMethod,

		AaMethod,
		AfValue,
		UseExtendedDepth,

		CsmEnabled,
		CsmDimension,
		CsmNumLayers,

		EnumSize
	};

	enum class RescaleMethod {
		/// Simple nearest-neighbour filtering (sharp)
		Nearest,
		/// Simple bilinear filtering (blurry)
		Linear,
		/// Cubic filtering (less blurry)
		Cubic,
		/// Advanced resampling algorithm
		Advanced
	};

	enum class AaMethod {
		/// No AA method is used (1 sample per pixel)
		None,
		/// Multisampling with 2 samples per pixel
		Msaa2x,
		/// Multisampling with 4 samples per pixel
		Msaa4x,
		/// Multisampling with 8 samples per pixel
		Msaa8x,
		/// Temporal anti-aliasing with 1 spatial and 2 temporal samples
		Taa1S2T,
		/// Temporal anti-aliasing with 1 spatial and 4 temporal samples
		Taa1S4T,
		/// Temporal anti-aliasing with 1 spatial and 8 temporal samples
		Taa1S8T
	};

	using FrameSize = std::pair<int32_t, int32_t>;
	using OptionChangedListener = void(*)(Option);

	// Call this when rendering subsystem is online
	// so we can query it's capabilities and limits
	void scanOptionsSupport();

	// Option getters

	FrameSize frameSize() const noexcept { return m_values.frame_size; }
	float sceneResolutionScale() const noexcept { return m_values.scene_resolution_scale; }
	RescaleMethod sceneRescaleMethod() const noexcept { return m_values.scene_rescale_method; }

	AaMethod aaMethod() const noexcept { return m_values.aa_method; }
	int32_t afValue() const noexcept { return m_values.af_value; }
	bool useExtendedDepth() const noexcept { return m_values.use_extended_depth; }

	bool csmEnabled() const noexcept { return m_values.csm_enabled; }
	int32_t csmDimension() const noexcept { return m_values.csm_dimension; }
	int32_t csmNumLayers() const noexcept { return m_values.csm_num_layers; }

	// Additional option getters

	std::vector<FrameSize> availableFrameSizes() const;
	std::pair<float, float> sceneResolutionScaleRange() const;
	bool isSceneRescaleMethodSupported(RescaleMethod method) const;

	bool aaMethodSupported(AaMethod method) const;
	bool afValueSuppored(int32_t value) const;
	bool isExtendedDepthSupported() const;

	bool isCsmSupported() const;
	std::pair<int32_t, int32_t> csmDimensionRange() const;
	std::pair<int32_t, int32_t> csmNumLayersRange() const;

	// Option setters

	[[nodiscard]] bool setFrameSize(std::pair<int32_t, int32_t> value);
	[[nodiscard]] bool setSceneResolutionScale(float value);
	[[nodiscard]] bool setSceneRescaleMethod(RescaleMethod value);

	[[nodiscard]] bool setAaMethod(AaMethod value);
	[[nodiscard]] bool setAfValue(int value);
	[[nodiscard]] bool setUseExtendedDepth(bool value);

	[[nodiscard]] bool setCsmEnabled(bool value);
	[[nodiscard]] bool setCsmDimension(int32_t value);
	[[nodiscard]] bool setCsmNumLayers(int32_t value);

private:
	struct {
		/// Dimensions of output frames (UI is rendered in this resolution)
		FrameSize frame_size { 1600, 900 };
		/// Scale of scene resolution relative to frame:
		/// - =1.0 -- =100%, scene has the same resolution as UI
		/// - <1.0 -- <100%, scene resolution is less than UI (upscaling will be performed)
		/// - >1.0 -- >100%, scene resolution is higher than UI (downscaling will be performed)
		float scene_resolution_scale = 1.0f;
		/// Used method of rescaling the scene (when scale is not 100%)
		RescaleMethod scene_rescale_method = RescaleMethod::Linear;

		/// Used anti-aliasing method
		AaMethod aa_method = AaMethod::None;
		/// Value of anisotropic filtering (1 means AF is disabled)
		int32_t af_value = 1;
		/// When `true`, depth range can exceed 0..1 range and 32-bit depth buffer is used
		bool use_extended_depth = false;

		/// Whether to use cascaded shadow maps (for sun shadows)
		bool csm_enabled = false;
		/// Size of one layer of cascaded shadow map (it's always square)
		int32_t csm_dimension = 2048;
		/// Number of cascaded shadow map layers
		int32_t csm_num_layers = 4;
	} m_values;
};

}
