#pragma once

#include <voxen/gfx/frame_tick_id.hpp>
#include <voxen/gfx/vk/vk_debug_utils.hpp>
#include <voxen/gfx/vk/vk_physical_device.hpp>
#include <voxen/gfx/vk/vma_fwd.hpp>

#include <extras/source_location.hpp>

#include <array>
#include <span>
#include <variant>
#include <vector>

namespace voxen::gfx::vk
{

class Instance;

struct DeviceDispatchTable {
#define VK_API_ENTRY(x) PFN_##x x = nullptr;
#include "api_device.in"
#undef VK_API_ENTRY
};

// NOTE: this object includes `PhysicalDevice` as a subobject so it's quite large too.
class Device {
public:
	enum Queue : uint32_t {
		// Supports GRAPHICS, COMPUTE (and TRANSFER)
		// operations. Always a dedicated queue.
		QueueMain = 0,
		// Supports TRANSFER operations.
		// Might be either a dedicated queue or an alias of main.
		QueueDma,
		// Supports COMPUTE (and TRANSFER) operations.
		// Might be either a dedicated queue or an alias of main.
		QueueCompute,

		// Do not use, only for counting queue kinds
		QueueCount,
	};

	// Compact (with bit fields packing where possible) representation of
	// device information.
	// Mostly duplicates information already exposed by `PhysicalDevice`
	// but allows for more concise and efficient checks (i.e. less logic
	// and memory accesses navigating through fields of that fat boy).
	struct Info {
		// Family index of `QueueMain`, for ownership transfer purposes
		uint32_t main_queue_family = 0;
		// Family index of `QueueDma`, can be equal to that of main
		uint32_t dma_queue_family = 0;
		// Family index of `QueueCompute`, can be equal to that of main
		uint32_t compute_queue_family = 0;

		// Set if VK_EXT_memory_budget is enabled for this device
		uint32_t have_memory_budget : 1 = 0;
		// Set if VK_EXT_mesh_shader is enabled for this device
		uint32_t have_mesh_shader : 1 = 0;

		// Set if DMA queue is not an alias of the main one
		uint32_t dedicated_dma_queue : 1 = 0;
		// Set if compute queue is not an alias of the main one
		uint32_t dedicated_compute_queue : 1 = 0;

		// Number of valid entries in `unique_queue_families`.
		// Can be supplied to e.g. `VkBufferCreateInfo::queueFamilyIndexCount.
		uint32_t unique_queue_family_count = 0;
		// Can be supplied to e.g. `VkBufferCreateInfo::pQueueFamilyIndices`
		uint32_t unique_queue_families[QueueCount] = {};
	};

	// See `submitCommands()`
	struct SubmitInfo {
		// Which queue to submit commands into
		Queue queue = QueueMain;

		// Timeline value(s) to wait for before starting the GPU work, paired with their queues.
		// Wait for a value which has no pending or complete signal operation *WILL* deadlock.
		// Empty span means no wait, execution begins as soon as it can.
		// NOTE: each queue has its own timeline, make sure you don't mix them.
		std::span<std::pair<Queue, uint64_t>> wait_timelines = {};
		// Binary semaphore to wait on before starting the GPU work.
		// As always with binary semaphores, it will be reset after waiting.
		// It must be either signaled or have a pending signal operation.
		VkSemaphore wait_binary_semaphore = VK_NULL_HANDLE;

		// Command buffers, will be submitted back-to-back without
		// synchronization in between. Can be empty (pure sync submit).
		std::span<const VkCommandBuffer> cmds = {};

		// Whether to signal a binary semaphore after GPU work completion.
		// It must be either unsignaled or have a pending wait operation.
		VkSemaphore signal_binary_semaphore = VK_NULL_HANDLE;
		// Whether to signal a fence after GPU work completion.
		// It must be unsignaled and have no pending signal operations.
		VkFence signal_fence = VK_NULL_HANDLE;
	};

	// Constructor will check if the device passes minimal requirements
	// (same as `isSupported()` call), throwing `VoxenErrc::GfxCapabilityMissing`
	// if it doesn't. Every supported (known) extension will be enabled.
	explicit Device(Instance &instance, PhysicalDevice &phys_dev);
	Device(Device &&) = delete;
	Device(const Device &) = delete;
	Device &operator=(Device &&) = delete;
	Device &operator=(const Device &) = delete;
	~Device() noexcept;

	// Submit work for GPU execution. See `SubmitInfo` for details.
	// Returns timeline value assigned to this submission. It will be greater than
	// the previous value returned for this queue by one. This value can be used
	// to synchronize further submissions or to wait for it on CPU.
	//
	// NOTE: each logical queue has its own timeline, even when it actually
	// aliases another queue. When waiting on the returned value, make sure
	// you always pair it with the same queue you were submitting to.
	//
	// Upon device failure (GPU hang etc.) this function is very likely
	// to be the first to throw `VulkanException` with `VK_ERROR_DEVICE_LOST`.
	uint64_t submitCommands(SubmitInfo info);

	// Wait (block) until a given queue's timeline value is signaled as complete.
	// Passing any value not returned from `submitCommands()` (with the
	// same queue specified) earlier *WILL* deadlock the program.
	//
	// NOTE: timeline is paired with the queue. Make sure this value
	// originates from submitting to `queue` and not something else.
	//
	// Upon device failure (GPU hang etc.) this function is very likely
	// to be the first to throw `VulkanException` with `VK_ERROR_DEVICE_LOST`.
	void waitForTimeline(Queue queue, uint64_t value);

	// Wait (block) until every queue's timeline value is signaled as complete.
	// Passing any value not returned from `submitCommands()` or `getLastSubmittedTimeline()`
	// for the respective queue earlier *WILL* deadlock the program.
	//
	// Timelines in `values` must be ordered to follow `Queue` enum values.
	//
	// Upon device failure (GPU hang etc.) this function is very likely
	// to be the first to throw `VulkanException` with `VK_ERROR_DEVICE_LOST`.
	void waitForTimelines(std::span<const uint64_t, QueueCount> values);

	// Get the last timeline value returned from `submitCommands()` to `queue`.
	// Returns zero if nothing was ever submitted to the queue.
	uint64_t getLastSubmittedTimeline(Queue queue) const noexcept { return m_last_submitted_timelines[queue]; }

	// Get the last completed (on GPU) timeline value for `queue`, does not wait
	uint64_t getCompletedTimeline(Queue queue);

	// Call `vkDeviceWaitIdle` to force completion of any pending GPU work.
	// Intended to be used only in object destructors. Any error
	// is only logged and ignored, so the function is nothrow.
	void forceCompletion() noexcept;

	void onFrameTickBegin(FrameTickId completed_tick, FrameTickId new_tick);
	void onFrameTickEnd(FrameTickId current_tick);

	// All functions of `enqueueDestroy` family work similarly - schedule
	// an object for deletion after the current frame tick ID completes executing
	// all GPU commands submitted during that frame. You can still use the object,
	// and even record and submit GPU commands referencing it, after the call
	// returns but before the current frame tick ends.

	void enqueueDestroy(VkBuffer buffer, VmaAllocation alloc) { enqueueJunkItem(std::pair { buffer, alloc }); }
	void enqueueDestroy(VkImage image, VmaAllocation alloc) { enqueueJunkItem(std::pair { image, alloc }); }
	void enqueueDestroy(VkImageView view) { enqueueJunkItem(view); }
	void enqueueDestroy(VkCommandPool pool) { enqueueJunkItem(pool); }
	void enqueueDestroy(VkDescriptorPool pool) { enqueueJunkItem(pool); }
	void enqueueDestroy(VkSwapchainKHR swapchain) { enqueueJunkItem(swapchain); }
	void enqueueDestroy(VkSampler sampler) { enqueueJunkItem(sampler); }

	// Shorthand to `.instance().debug().setObjectName()`
	void setObjectName(uint64_t handle, VkObjectType type, const char *name) noexcept;

	template<typename T>
	void setObjectName(T handle, const char *name) noexcept
	{
		setObjectName(reinterpret_cast<uint64_t>(handle), DebugUtils::objectType<T>(), name);
	}

	Instance &instance() noexcept { return m_instance; }
	PhysicalDevice &physicalDevice() noexcept { return m_phys_device; }
	// Shorthand to `.instance().debug()`
	DebugUtils &debug() noexcept;

	VkDevice handle() const noexcept { return m_handle; }
	VmaAllocator vma() const noexcept { return m_vma; }

	VkQueue queue(Queue index) const noexcept { return m_queues[index]; }
	VkQueue mainQueue() const noexcept { return m_queues[QueueMain]; }
	VkQueue dmaQueue() const noexcept { return m_queues[QueueDma]; }
	VkQueue computeQueue() const noexcept { return m_queues[QueueCompute]; }

	const Info &info() const noexcept { return m_info; }
	const PhysicalDevice::Info &physInfo() const noexcept { return m_phys_device.info(); }

	const DeviceDispatchTable &dt() const noexcept { return m_dt; }

	// Check if a given physical device passes minimal requirements
	// to create VkDevice (this class instance) from it. Details of
	// this check will be logged with debug level.
	static bool isSupported(PhysicalDevice &pd);

	// More convenient interfaces to certain Vulkan functions which
	// convert error codes to exceptions and can assign debug names
#pragma region Vulkan API wrappers
	using SLoc = extras::source_location;

	VkImageView vkCreateImageView(const VkImageViewCreateInfo &create_info, const char *name = nullptr,
		SLoc loc = SLoc::current());
	VkSampler vkCreateSampler(const VkSamplerCreateInfo &create_info, const char *name = nullptr,
		SLoc loc = SLoc::current());
	VkSemaphore vkCreateSemaphore(const VkSemaphoreCreateInfo &create_info, const char *name = nullptr,
		SLoc loc = SLoc::current());
	VkSwapchainKHR vkCreateSwapchain(const VkSwapchainCreateInfoKHR &create_info, SLoc loc = SLoc::current());

	void vkDestroyImageView(VkImageView view) noexcept;
	void vkDestroySemaphore(VkSemaphore semaphore) noexcept;
	void vkDestroySwapchain(VkSwapchainKHR swapchain) noexcept;

	void vkUpdateDescriptorSets(uint32_t num_writes, const VkWriteDescriptorSet *writes, uint32_t num_copies = 0,
		const VkCopyDescriptorSet *copies = nullptr) noexcept;
#pragma endregion

private:
	using JunkItem = std::variant<std::pair<VkBuffer, VmaAllocation>, std::pair<VkImage, VmaAllocation>, VkImageView,
		VkCommandPool, VkDescriptorPool, VkSwapchainKHR, VkSampler>;
	using JunkEnqueue = std::pair<JunkItem, FrameTickId>;

	Instance &m_instance;

	VkDevice m_handle = VK_NULL_HANDLE;
	VmaAllocator m_vma = VK_NULL_HANDLE;

	VkSemaphore m_timeline_semaphores[QueueCount] = {};
	VkQueue m_queues[QueueCount] = {};
	uint64_t m_last_submitted_timelines[QueueCount] = {};
	uint64_t m_last_completed_timelines[QueueCount] = {};

	FrameTickId m_current_tick_id = FrameTickId::INVALID;
	std::vector<JunkEnqueue> m_destroy_queue;

	Info m_info;

	DeviceDispatchTable m_dt;

	// Place this fat boy at the end
	PhysicalDevice m_phys_device;

	void createDevice();
	void getQueueHandles();
	void createVma();
	void createTimelineSemaphores();
	void processDestroyQueue(FrameTickId completed_tick) noexcept;

	void enqueueJunkItem(JunkItem item);
	void destroy(std::pair<VkBuffer, VmaAllocation> &obj) noexcept;
	void destroy(std::pair<VkImage, VmaAllocation> &obj) noexcept;
	void destroy(VkImageView obj) noexcept;
	void destroy(VkCommandPool pool) noexcept;
	void destroy(VkDescriptorPool pool) noexcept;
	void destroy(VkSwapchainKHR swapchain) noexcept;
	void destroy(VkSampler sampler) noexcept;
};

} // namespace voxen::gfx::vk
