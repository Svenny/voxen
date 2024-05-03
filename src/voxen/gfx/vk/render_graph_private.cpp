#include "render_graph_private.hpp"

namespace voxen::gfx::vk
{

RenderGraphPrivate::~RenderGraphPrivate() noexcept
{
	// Break links to public objects and destroy handles

	for (auto &buffer : buffers) {
		if (buffer.resource) {
			*buffer.resource = RenderGraphBuffer();
		}

		// TODO: destroy handle
	}

	for (auto &image : images) {
		if (image.resource) {
			*image.resource = RenderGraphImage();
		}

		// TODO: destroy handle

		for (auto &view : image.views) {
			if (view.resource) {
				*view.resource = RenderGraphImageView();
			}

			// TODO: destroy handle
		}
	}
}

} // namespace voxen::gfx::vk
