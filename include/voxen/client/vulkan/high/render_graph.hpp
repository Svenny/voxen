#pragma once

#include <voxen/client/vulkan/capabilities.hpp>
#include <voxen/client/vulkan/render_pass.hpp>
#include <voxen/client/vulkan/high/per_frame_resources.hpp>

#include <voxen/client/graphics_options.hpp>

#include <array>
#include <vector>
#include <unordered_map>

namespace voxen::client::vulkan
{

enum class RenderStage : uint32_t {
	// Does not denote any stage
	None = 0,
	SceneHdrDrawPass,
	SceneHdrResolvePass,
	OitFullResPass,
	OitHalfResPass,
	HdrResolvePass,
	FinalPass,

	EnumMax
};

class RenderGraph {
public:
	constexpr inline static VkFormat SCENE_HDR_COLOR_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
	constexpr inline static VkFormat SCENE_FINAL_COLOR_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
	constexpr inline static VkFormat SCENE_DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;
	constexpr inline static VkFormat OIT_ACCUM_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
	constexpr inline static VkFormat OIT_REVEAL_FORMAT = VK_FORMAT_R16_UNORM;

	constexpr inline static uint32_t ATTACHMENT_SLOT_NA = UINT32_MAX;
	constexpr inline static uint32_t ATTACHMENT_SLOT_ZS = ATTACHMENT_SLOT_NA - 1;

	enum DynamicConfig : uint32_t {
		DYNAMIC_CONFIG_HAS_FULL_RES_TRANSPARENCY_BIT = 0x0001,
		DYNAMIC_CONFIG_HAS_HALF_RES_TRANSPARENCY_BIT = 0x0002,

		// This value is purely internal and must not be used anywhere
		DYNAMIC_CONFIG_MAX_VALUE,
		DYNAMIC_CONFIG_BIT_WIDTH = 2 * (DYNAMIC_CONFIG_MAX_VALUE - 1)
	};

	using DynamicCondition = bool (*)(DynamicConfig config) noexcept;

	enum class TargetFormatClass {
		// Special invalid value, will trigger graph compile error if met in description
		Invalid,
		// Format of HDR scene color buffer
		SceneHdrColor,
		// Format of LDR/resolved scene color buffer
		SceneFinalColor,
		// Format of scene depth/stencil buffer
		SceneDepthStencil,
		// Format of OIT accumulator buffer
		OitAccum,
		// Format of OIT reveal buffer
		OitReveal,
		// Format of swapchain images
		Swapchain
	};

	enum class TargetDimensionsClass {
		// Special invalid value, will trigger graph compile error if met in description
		Invalid,
		// The target is a 2D resource which size is equal to that of scene color buffer
		Scene,
		// The target is a 2D resource which size is equal to that of swapchain image
		Window,
		// The target is a 2D resource half the size of scene color buffer
		HalfScene
	};

	enum class TargetSamplesClass {
		// Special invalid value, will trigger graph compile error if met in description
		Invalid,
		// The target always has one sample or is not a multisample-capable resource
		One,
		// The target has N samples as set by used AA method's spatial samples
		ByAaMethod
	};

	struct TargetProperties {
		bool (*dynamic_condition)(DynamicConfig config) noexcept = nullptr;
		TargetFormatClass format;
		TargetDimensionsClass dimensions;
		TargetSamplesClass samples = TargetSamplesClass::One;
	};

	enum class TargetReadType {
		ReadOnlyAttachment,
		InputAttachment,
		SampledImage,
		ReadOnlyStorageImage
	};

	struct TargetReference {
		RenderTarget target;
		uint32_t mip_level = 0;
		uint32_t array_layer = 0;
	};

	struct StageRequires {
		RenderTarget target;
		TargetReadType type;
		uint32_t attachment_slot = ATTACHMENT_SLOT_NA;
		uint32_t mip_level = 0;
		uint32_t array_layer = 0;
		uint32_t min_temporal_offset = 0;
		uint32_t max_temporal_offset = 0;
	};

	enum class TargetWriteType {
		Attachment,
		ResolveAttachment,
		StorageImage
	};

	struct StageProvides {
		RenderTarget target;
		TargetWriteType type;
		uint32_t attachment_slot = ATTACHMENT_SLOT_NA;
		uint32_t mip_level = 0;
		uint32_t array_layer = 0;
	};

	struct StageProperties {
		DynamicCondition dynamic_condition = nullptr;
		uint32_t order = UINT32_MAX;

		std::vector<StageRequires> requires_targets;
		std::vector<StageProvides> provides_targets;

		// Input attachments (always have read-only access)
		std::array<RenderTarget, Capabilities::NUM_REQUIRED_COLOR_ATTACHMENTS> input_attachments;
		// Color attachments (can have read-only or read-write access)
		std::array<RenderTarget, Capabilities::NUM_REQUIRED_COLOR_ATTACHMENTS> color_attachments;
		// Resolve attachments (always have read-write access)
		std::array<RenderTarget, Capabilities::NUM_REQUIRED_COLOR_ATTACHMENTS> resolve_attachments;

		// Depth-stencil attachment
		RenderTarget zs_attachment = RenderTarget::None;
		// Depth-stencil resolve attachment
		RenderTarget zs_resolve_attachment = RenderTarget::None;

		// i-th element denotes whether i-th color attachment is read-only
		std::array<bool, Capabilities::NUM_REQUIRED_COLOR_ATTACHMENTS> color_attachments_readonly;
		// Is depth-stencil attachment read-only
		bool zs_attachment_readonly;
	};

	struct GraphicsStageProperties {
		DynamicCondition dynamic_condition = nullptr;
		uint32_t order = UINT32_MAX;

		// Input attachments (always have read-only access)
		std::array<RenderTarget, Capabilities::NUM_REQUIRED_COLOR_ATTACHMENTS> input_attachments;
		// Color attachments (can have read-only or read-write access)
		std::array<RenderTarget, Capabilities::NUM_REQUIRED_COLOR_ATTACHMENTS> color_attachments;
		// Resolve attachments (always have read-write access)
		std::array<RenderTarget, Capabilities::NUM_REQUIRED_COLOR_ATTACHMENTS> resolve_attachments;

		// Depth-stencil attachment
		RenderTarget zs_attachment = RenderTarget::None;
		// Depth-stencil resolve attachment
		RenderTarget zs_resolve_attachment = RenderTarget::None;

		// i-th element denotes whether i-th color attachment is read-only
		std::array<bool, Capabilities::NUM_REQUIRED_COLOR_ATTACHMENTS> color_attachments_readonly;
		// Is depth-stencil attachment read-only
		bool zs_attachment_readonly;
	};

	RenderGraph() = default;
	RenderGraph(RenderGraph &&) = delete;
	RenderGraph(const RenderGraph &) = delete;
	RenderGraph &operator = (RenderGraph &&) = delete;
	RenderGraph &operator = (const RenderGraph &) = delete;
	~RenderGraph() = default;

	void rebuild(const GraphicsOptions &opts);

	std::pair<VkRenderPass, uint32_t> getRenderStageLocation(RenderStage stage) const;

	static const char *getStageName(RenderStage stage) noexcept;

private:
	std::array<RenderPass, DYNAMIC_CONFIG_BIT_WIDTH> m_main_pass;

	void createMainPass(const GraphicsOptions &opts, DynamicConfig config);

	void fillRenderGraph(const GraphicsOptions &opts);

	std::unordered_map<RenderTarget, TargetProperties> m_target_props;
	std::unordered_map<RenderStage, StageProperties> m_stage_props;
};

}
