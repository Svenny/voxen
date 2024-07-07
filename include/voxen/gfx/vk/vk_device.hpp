#pragma once

#include <voxen/gfx/vk/vk_debug_utils.hpp>
#include <voxen/gfx/vk/vk_physical_device.hpp>

using VmaAllocation = struct VmaAllocation_T *;
using VmaAllocator = struct VmaAllocator_T *;

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
	// Compact (with bit fields packing where possible) representation of
	// device information.
	// Mostly duplicates information already exposed by `PhysicalDevice`
	// but allows for more concise and efficient checks (i.e. less logic
	// and memory accesses navigating through fields of that fat boy).
	struct Info {
		// Family index of `SubmitQueue::Main`, for ownership transfer purposes
		uint32_t main_queue_family = 0;
		// Family index of `SubmitQueue::Compute`, can be equal to that of main
		uint32_t compute_queue_family = 0;
		// Family index of `SubmitQueue::Dma`, can be equal to that of main
		uint32_t dma_queue_family = 0;

		// Set if VK_EXT_memory_budget is enabled for this device
		uint32_t have_memory_budget : 1 = 0;
		// Set if VK_EXT_mesh_shader is enabled for this device
		uint32_t have_mesh_shader : 1 = 0;

		// Set if compute queue is not an alias of the main one
		uint32_t dedicated_compute_queue : 1 = 0;
		// Set if DMA queue is not an aliasa of the main one
		uint32_t dedicated_dma_queue : 1 = 0;
	};

	enum class SubmitQueue {
		// Supports GRAPHICS, COMPUTE (and TRANSFER)
		// operations. Always a dedicated queue.
		Main,
		// Supports COMPUTE (and TRANSFER) operations.
		// Might be either a dedicated queue or an alias of main.
		Compute,
		// Supports TRANSFER operations.
		// Might be either a dedicated queue or an alias of main.
		Dma
	};

	// See `submitCommands()`
	struct SubmitInfo {
		// Which queue to submit commands into
		SubmitQueue queue = SubmitQueue::Main;

		// Timeline value to wait for before starting the GPU work.
		// 0 means no wait, execution begins as soon as it can.
		uint64_t wait_timeline = 0;
		// Binary semaphore to wait on before starting the GPU work.
		// As always with binary semaphores, it will be reset after waiting.
		// It must be either signaled or have a pending signal operation.
		VkSemaphore wait_binary_semaphore = VK_NULL_HANDLE;

		// Command buffers, will be submitted back-to-back without
		// synchronization in between. Can be empty (pure sync submit).
		std::span<const VkCommandBuffer> cmds;

		// Whether to signal a timeline value after GPU work completion.
		// Signaled value will be returned from `submitCommands()`.
		bool signal_timeline = false;
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
	// Returns timeline value assigned to this submission (zero if
	// no timeline signaling was requested). This value can be used
	// to synchronize further submissions or to wait for it on CPU.
	//
	// Upon device failure (GPU hang etc.) this function is very likely
	// to be the first to throw `VulkanException` with `VK_ERROR_DEVICE_LOST`.
	uint64_t submitCommands(SubmitInfo info);
	// Wait (block) until a given timeline value is signaled as complete.
	// Passing any value not returned from `submitCommands()` earlier
	// *WILL* deadlock the program.
	//
	// Upon device failure (GPU hang etc.) this function is very likely
	// to be the first to throw `VulkanException` with `VK_ERROR_DEVICE_LOST`.
	void waitForTimeline(uint64_t value);

	// All functions of `enqueueDestroy` family work similarly - schedule
	// an object for deletion after the last GPU command submission
	// completes execution. Completion can happen at any moment, even
	// right inside this function call, so basically it is only a
	// synchronized `vkDestroy*`. Therefore, any command buffers
	// referencing the object are considered invalid after this call.

	void enqueueDestroy(VkBuffer buffer, VmaAllocation alloc) { enqueueJunkItem(std::pair { buffer, alloc }); }
	void enqueueDestroy(VkImage image, VmaAllocation alloc) { enqueueJunkItem(std::pair { image, alloc }); }
	void enqueueDestroy(VkImageView view) { enqueueJunkItem(view); }
	void enqueueDestroy(VkCommandPool pool) { enqueueJunkItem(pool); }
	void enqueueDestroy(VkDescriptorPool pool) { enqueueJunkItem(pool); }

	// Shorthand to `.instance().debug().setObjectName()`
	void setObjectName(uint64_t handle, VkObjectType type, const char *name);

	template<typename T>
	void setObjectName(T handle, const char *name)
	{
		setObjectName(reinterpret_cast<uint64_t>(handle), DebugUtils::objectType<T>(), name);
	}

	Instance &instance() noexcept { return m_instance; }
	PhysicalDevice &physicalDevice() noexcept { return m_phys_device; }
	// Shorthand to `.instance().debug()`
	DebugUtils &debug() noexcept;

	VkDevice handle() const noexcept { return m_handle; }
	VmaAllocator vma() const noexcept { return m_vma; }

	VkQueue mainQueue() const noexcept { return m_main_queue; }
	VkQueue computeQueue() const noexcept { return m_compute_queue; }
	VkQueue dmaQueue() const noexcept { return m_dma_queue; }

	const Info &info() const noexcept { return m_info; }
	const PhysicalDevice::Info &physInfo() const noexcept { return m_phys_device.info(); }

	const DeviceDispatchTable &dt() const noexcept { return m_dt; }

	// Check if a given physical device passes minimal requirements
	// to create VkDevice (this class instance) from it. Details of
	// this check will be logged with debug level.
	static bool isSupported(PhysicalDevice &pd);

private:
	using JunkItem = std::variant<std::pair<VkBuffer, VmaAllocation>, std::pair<VkImage, VmaAllocation>, VkImageView,
		VkCommandPool, VkDescriptorPool>;

	Instance &m_instance;

	VkDevice m_handle = VK_NULL_HANDLE;
	VmaAllocator m_vma = VK_NULL_HANDLE;
	VkSemaphore m_timeline_semaphore = VK_NULL_HANDLE;

	VkQueue m_main_queue = VK_NULL_HANDLE;
	VkQueue m_compute_queue = VK_NULL_HANDLE;
	VkQueue m_dma_queue = VK_NULL_HANDLE;

	uint64_t m_last_submitted_timeline = 0;
	uint64_t m_last_completed_timeline = 0;
	std::vector<std::pair<JunkItem, uint64_t>> m_destroy_queue;

	Info m_info;

	DeviceDispatchTable m_dt;

	// Place this fat boy at the end
	PhysicalDevice m_phys_device;

	void createDevice();
	void getQueueHandles();
	void createVma();
	void createTimelineSemaphore();
	void processDestroyQueue();

	void enqueueJunkItem(JunkItem item);
	void destroy(std::pair<VkBuffer, VmaAllocation> &obj) noexcept;
	void destroy(std::pair<VkImage, VmaAllocation> &obj) noexcept;
	void destroy(VkImageView obj) noexcept;
	void destroy(VkCommandPool pool) noexcept;
	void destroy(VkDescriptorPool pool) noexcept;
};

} // namespace voxen::gfx::vk
