#pragma once

#include <voxen/client/vulkan/high/render_graph.hpp>

namespace voxen::client::vulkan
{

class RenderPassBuilder {
public:

	struct SubpassInfo {

	};

	void setPassParameters(RenderGraph::TargetDimensionsClass dimensions);
	uint32_t addSubpass();

private:

};

}
