#include <voxen/gfx/vk/render_graph_execution.hpp>

#include <voxen/util/log.hpp>

#include "render_graph_private.hpp"

namespace voxen::gfx::vk
{

RenderGraphExecution::RenderGraphExecution(RenderGraphPrivate &priv) noexcept : m_private(priv)
{
	(void) m_private;
}

RenderGraphExecution::~RenderGraphExecution() noexcept = default;

void RenderGraphExecution::setDynamicBufferSize(RenderGraphBuffer &buffer, VkDeviceSize size)
{
	auto *priv = buffer.getPrivate();
	assert(priv && priv->dynamic_sized && priv->used_size == 0);

	priv->used_size = size;

	if (priv->create_info.size >= size) {
		// Enough space in current allocation
		return;
	}

	// Not enough space, need to reallocate buffer
	priv->create_info.size = size;

	// TODO: do reallocation
}

} // namespace voxen::gfx::vk
