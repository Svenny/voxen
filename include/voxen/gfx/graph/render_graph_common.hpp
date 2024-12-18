#pragma once

#include <voxen/gfx/gfx_fwd.hpp>
#include <voxen/visibility.hpp>

namespace voxen::gfx::graph
{

class VOXEN_API IRenderPass {
	IRenderPass() = default;
	IRenderPass(IRenderPass &&) = default;
	IRenderPass(const IRenderPass &) = default;
	IRenderPass &operator=(IRenderPass &&) = default;
	IRenderPass &operator=(const IRenderPass &) = default;
	virtual ~IRenderPass() noexcept;
};

class RenderGraphBuffer {};
class RenderGraphImage {};

enum RenderPassType {
	Graphcs,
	Compute,
	SubGraph,
};

struct RenderPassDescription {

};

template<typename T>
class RenderPassHandle {};

} // namespace voxen::gfx::graph
