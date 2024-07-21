#include "render_graph_private.hpp"

namespace voxen::gfx::vk
{

RenderGraphPrivate::~RenderGraphPrivate() noexcept
{
	clear();
}

void RenderGraphPrivate::clear() noexcept
{
	// Drop internal objects
	commands.clear();

	assert(!output_image.resource);
	output_image = {};
	assert(!output_rtv.resource);
	output_rtv = {};

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

	buffers.clear();
	images.clear();
}

} // namespace voxen::gfx::vk
