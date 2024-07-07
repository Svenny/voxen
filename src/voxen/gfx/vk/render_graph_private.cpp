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

		device.enqueueDestroy(buffer.handle, buffer.alloc);
	}

	for (auto &image : images) {
		if (image.resource) {
			*image.resource = RenderGraphImage();
		}

		device.enqueueDestroy(image.handle, image.alloc);

		for (auto &view : image.views) {
			if (view.resource) {
				*view.resource = RenderGraphImageView();
			}

			device.enqueueDestroy(view.handle);
		}
	}
}

} // namespace voxen::gfx::vk
