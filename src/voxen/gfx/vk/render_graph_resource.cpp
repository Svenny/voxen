#include <voxen/gfx/vk/render_graph_resource.hpp>

#include "render_graph_private.hpp"

#include <cassert>

namespace voxen::gfx::vk
{

// RenderGraphBuffer

RenderGraphBuffer::RenderGraphBuffer(Private &priv) noexcept : m_private(&priv)
{
	assert(!priv.resource);
	priv.resource = this;
}

RenderGraphBuffer::RenderGraphBuffer(RenderGraphBuffer &&other) noexcept
{
	*this = std::move(other);
}

RenderGraphBuffer &RenderGraphBuffer::operator=(RenderGraphBuffer &&other) noexcept
{
	std::swap(m_private, other.m_private);
	std::swap(m_handle, other.m_handle);

	if (m_private) {
		m_private->resource = this;
	}

	if (other.m_private) {
		other.m_private->resource = &other;
	}

	return *this;
}

RenderGraphBuffer::~RenderGraphBuffer() noexcept
{
	if (m_private) {
		m_private->resource = nullptr;
	}
}

// RenderGraphImage

RenderGraphImage::RenderGraphImage(Private &priv) noexcept : m_private(&priv)
{
	assert(!priv.resource);
	priv.resource = this;
}

RenderGraphImage::RenderGraphImage(RenderGraphImage &&other) noexcept
{
	*this = std::move(other);
}

RenderGraphImage &RenderGraphImage::operator=(RenderGraphImage &&other) noexcept
{
	std::swap(m_private, other.m_private);
	std::swap(m_handle, other.m_handle);

	if (m_private) {
		m_private->resource = this;
	}

	if (other.m_private) {
		other.m_private->resource = &other;
	}

	return *this;
}

RenderGraphImage::~RenderGraphImage() noexcept
{
	if (m_private) {
		m_private->resource = nullptr;
	}
}

// RenderGraphImageView

RenderGraphImageView::RenderGraphImageView(Private &priv) noexcept : m_private(&priv)
{
	assert(!priv.resource);
	priv.resource = this;
}

RenderGraphImageView::RenderGraphImageView(RenderGraphImageView &&other) noexcept
{
	*this = std::move(other);
}

RenderGraphImageView &RenderGraphImageView::operator=(RenderGraphImageView &&other) noexcept
{
	std::swap(m_private, other.m_private);
	std::swap(m_handle, other.m_handle);

	if (m_private) {
		m_private->resource = this;
	}

	if (other.m_private) {
		other.m_private->resource = &other;
	}

	return *this;
}

RenderGraphImageView::~RenderGraphImageView() noexcept
{
	if (m_private) {
		m_private->resource = nullptr;
	}
}

} // namespace voxen::gfx::vk
