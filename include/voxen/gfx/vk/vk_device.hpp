#pragma once

#include <voxen/gfx/vk/vk_debug_utils.hpp>
#include <voxen/gfx/vk/vk_physical_device.hpp>
#include <voxen/gfx/vk/vma_fwd.hpp>

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

		// Set if DMA queue is not an aliasa of the main one
		uint32_t dedicated_dma_queue : 1 = 0;
		// Set if compute queue is not an alias of the main one
		uint32_t dedicated_compute_queue : 1 = 0;
	};

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

	// Call `vkDeviceWaitIdle` to force completion of any pending GPU work.
	// Intended to be used only in object destructors. Any error
	// is only logged and ignored, so the function is nothrow.
	void forceCompletion() noexcept;

	// All functions of `enqueueDestroy` family work similarly - schedule
	// an object for deletion after the (currently) last GPU command submission
	// on every queue completes execution. Completion can happen at any moment,
	// even right inside this function call, so basically it is nothing more
	// than a GPU-synchronized `vkDestroy*`. Therefore, any command buffers
	// referencing the object are considered invalid after this call.

	void enqueueDestroy(VkBuffer buffer, VmaAllocation alloc) { enqueueJunkItem(std::pair { buffer, alloc }); }
	void enqueueDestroy(VkImage image, VmaAllocation alloc) { enqueueJunkItem(std::pair { image, alloc }); }
	void enqueueDestroy(VkImageView view) { enqueueJunkItem(view); }
	void enqueueDestroy(VkCommandPool pool) { enqueueJunkItem(pool); }
	void enqueueDestroy(VkDescriptorPool pool) { enqueueJunkItem(pool); }
	void enqueueDestroy(VkSwapchainKHR swapchain) { enqueueJunkItem(swapchain); }

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
		VkCommandPool, VkDescriptorPool, VkSwapchainKHR>;
	using JunkEnqueue = std::pair<JunkItem, std::array<uint64_t, QueueCount>>;

	Instance &m_instance;

	VkDevice m_handle = VK_NULL_HANDLE;
	VmaAllocator m_vma = VK_NULL_HANDLE;

	VkSemaphore m_timeline_semaphores[QueueCount] = {};
	VkQueue m_queues[QueueCount] = {};
	uint64_t m_last_submitted_timelines[QueueCount] = {};
	uint64_t m_last_completed_timelines[QueueCount] = {};

	std::vector<JunkEnqueue> m_destroy_queue;

	Info m_info;

	DeviceDispatchTable m_dt;

	// Place this fat boy at the end
	PhysicalDevice m_phys_device;

	void createDevice();
	void getQueueHandles();
	void createVma();
	void createTimelineSemaphores();
	void processDestroyQueue() noexcept;

	void enqueueJunkItem(JunkItem item);
	void destroy(std::pair<VkBuffer, VmaAllocation> &obj) noexcept;
	void destroy(std::pair<VkImage, VmaAllocation> &obj) noexcept;
	void destroy(VkImageView obj) noexcept;
	void destroy(VkCommandPool pool) noexcept;
	void destroy(VkDescriptorPool pool) noexcept;
	void destroy(VkSwapchainKHR swapchain) noexcept;
};

} // namespace voxen::gfx::vk
