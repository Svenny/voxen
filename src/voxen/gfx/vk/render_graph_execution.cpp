#include <voxen/gfx/vk/render_graph_execution.hpp>

#include <voxen/client/vulkan/common.hpp>
#include <voxen/util/log.hpp>

#include "render_graph_private.hpp"

#include <vma/vk_mem_alloc.h>

namespace voxen::gfx::vk
{

// TODO: there parts are not yet moved to voxen/gfx/vk
using client::vulkan::VulkanException;

RenderGraphExecution::RenderGraphExecution(RenderGraphPrivate &priv) noexcept
	: m_private(priv), m_frame_context(priv.fctx_ring.current())
{}

RenderGraphExecution::~RenderGraphExecution() noexcept = default;

void RenderGraphExecution::setDynamicBufferSize(RenderGraphBuffer &buffer, VkDeviceSize size)
{
	auto *priv = buffer.getPrivate();
	assert(priv && priv->dynamic_sized && priv->used_size == 0);

	if (priv->handle != VK_NULL_HANDLE && priv->create_info.size >= size) {
		// Enough space in current allocation
		priv->used_size = size;
		buffer.setHandle(priv->handle);
		return;
	}

	// Not enough space, need to reallocate buffer
	m_private.device.enqueueDestroy(priv->handle, priv->alloc);
	priv->handle = VK_NULL_HANDLE;
	priv->alloc = VK_NULL_HANDLE;

	priv->create_info.size = size;

	VmaAllocationCreateInfo vma_info {};
	vma_info.usage = VMA_MEMORY_USAGE_AUTO;

	Device &dev = m_private.device;
	VmaAllocator vma = dev.vma();
	VkResult res = vmaCreateBuffer(vma, &priv->create_info, &vma_info, &priv->handle, &priv->alloc, nullptr);
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vmaCreateBuffer");
	}

	dev.setObjectName(priv->handle, priv->name.c_str());

	priv->used_size = size;
	buffer.setHandle(priv->handle);
}

} // namespace voxen::gfx::vk
