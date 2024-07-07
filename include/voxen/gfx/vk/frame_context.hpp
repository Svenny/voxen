#pragma once

#include <vulkan/vulkan.h>

#include <extras/dyn_array.hpp>

#include <span>
#include <vector>

namespace voxen::gfx::vk
{

class Device;

// Allocator of per-frame temporary resources.
//
// Features:
// - Single command buffer, recording is began/ended automatically
// - Temporary descriptor sets allocation
// - Temporary constant buffer uploads allocation
//
// Do not create this object manually, access it through `FrameContextRing::current()`.
//
// TODO features:
// - Async readback (`allocateReadbackTicket(size)` -> returns "buffer-promise" object)
// - Async queries, mostly timestamp (`writeTimestamp(...)` -> returns "timestamp-promise" object)
// - Performance stats (wait stalls, descriptor sets, constant upload allocation, etc.)
// - Multiple command buffers (parallel cmd recording/partial submissions/async compute etc.)
class FrameContext {
public:
	struct ConstantUpload {
		// Vulkan buffer handle of this buffer
		VkBuffer buffer = VK_NULL_HANDLE;
		// Offset (bytes) from the beginning of this buffer.
		// Use it in descriptor writes or dynamic offset bindings.
		VkDeviceSize offset = 0;
		// Host-visible memory span to write upload data to.
		// `offset` is already applied, don't do pointer math.
		std::span<std::byte> host_mapped_span;
	};

	explicit FrameContext(Device &device);
	FrameContext(FrameContext &&) = delete;
	FrameContext(const FrameContext &) = delete;
	FrameContext &operator=(FrameContext &&) = delete;
	FrameContext &operator=(const FrameContext &) = delete;
	~FrameContext() noexcept;

	// Command buffer assigned to this context.
	// It is in recording state (per Vulkan spec).
	// Do not end it manually - this will happen automatically.
	VkCommandBuffer commandBuffer() const noexcept { return m_cmd_buffer; }

	// Allocate a temporary descriptor set for given layout.
	// Returned handle is valid until this context is submitted to GPU.
	//
	// Strong exception safety - if it throws (most likely due to OOM),
	// the context state is not affected. It can throw if you try
	// to allocate an extremely big set, though this is not practically
	// possible except for bindless-style sets (like 10K textures etc).
	//
	// Bindless mega-sets must be managed differently
	// (they are not allocated once per frame to begin with),
	// this method is not the right tool for them.
	VkDescriptorSet allocateDescriptorSet(VkDescriptorSetLayout layout);
	// Allocate a temporary constant buffer slice for upload.
	// Returned slice is:
	// - Persistently host-mapped, likely with write-combined memory
	// - Has at least `size` bytes but belongs to a larger buffer
	// - Buffer has the only usage bit - VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
	// - Valid until this context is submitted to GPU
	//
	// Slices from consecutive allocations will usually belong to the
	// same buffer but sometimes can be in different ones.
	// This makes "opportunistic dynamic offsets" a viable strategy.
	// If you have series of draw/dispatch calls where the only changing descriptor
	// is constant buffer, you can make it VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
	// and check `buffer` handle returned by this method. Allocate a new
	// set only if it changes, write 0 in the descriptor offset and use
	// `offset` field as a dynamic offset.
	//
	// The context will automatically adjust internal buffer sizes later
	// attempting to serve every allocation from the same buffer.
	ConstantUpload allocateConstantUpload(VkDeviceSize size);

protected:
	uint64_t submit();
	void waitAndReset();

private:
	struct ConstantUploadBuffer;

	Device &m_device;

	VkCommandPool m_cmd_pool = VK_NULL_HANDLE;
	VkCommandBuffer m_cmd_buffer = VK_NULL_HANDLE;

	std::vector<VkDescriptorPool> m_descriptor_pools;
	std::vector<ConstantUploadBuffer> m_const_upload_buffers;

	uint64_t m_submit_timeline = 0;

	// Cached `minUniformBufferOffsetAlignment` from device properties
	VkDeviceSize m_ubo_offset_align = 0;

	void addDescriptorPool();
	void addConstantUploadBuffer(VkDeviceSize size);

	friend class FrameContextRing;
};

// Ring buffer of several `FrameContext` instances to pipeline CPU->GPU command submission.
class FrameContextRing {
public:
	// Size is fixed at creation time.
	// It should be at least 2, having one context ring makes no sense.
	// Increasing it makes CPU<->GPU latency bigger and reduces
	// the risk of GPU stall due to lack of workload (when not CPU-bound).
	// The generally recommended value is either 2 or 3.
	explicit FrameContextRing(Device &device, size_t size);
	FrameContextRing(FrameContextRing &&) = delete;
	FrameContextRing(const FrameContextRing &) = delete;
	FrameContextRing &operator=(FrameContextRing &&) = delete;
	FrameContextRing &operator=(const FrameContextRing &) = delete;
	~FrameContextRing() noexcept = default;

	// Does, in order:
	// 1. End recording the command buffer of `current()` context
	// 2. Submit recorded commands for GPU execution
	// 3. Advance ring buffer pointer, `current()` changes value here
	// 4. Wait for GPU completion of the new `current()` context
	// 5. Reset all temporary objects allocated in it
	//
	// Returns device timeline value assigned to submission (from step 2).
	//
	// References returned by `current()` can't be used after this call,
	// even if they are technically still valid. This disallows touching
	// contexts with pending GPU work.
	//
	//	If this method throws, then something is really screwed (most likely
	// device loss or OOM) and using frame contexts is no longer possible.
	// No memory/objects are leaked, but the ring enters "bad state"
	// and can only be destroyed. `current()` will return undefined values after that.
	uint64_t submitAndAdvance();

	// Returns `true` if the ring is in "bad state", which can only
	// (and always will) happen if `submitAndAdvance()` fails.
	// In bad state `current()` returns undefined references.
	bool badState() const noexcept { return m_contexts.empty(); }

	FrameContext &current() noexcept { return m_contexts[m_current]; }
	const FrameContext &current() const noexcept { return m_contexts[m_current]; }

private:
	extras::dyn_array<FrameContext> m_contexts;
	size_t m_current = 0;
};

} // namespace voxen::gfx::vk
