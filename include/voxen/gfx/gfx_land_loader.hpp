#pragma once

#include <voxen/gfx/gfx_fwd.hpp>
#include <voxen/land/chunk_key.hpp>
#include <voxen/svc/svc_fwd.hpp>
#include <voxen/world/world_fwd.hpp>

#include <extras/pimpl.hpp>

#include <glm/vec3.hpp>

#include <vector>

typedef struct VkBuffer_T *VkBuffer;
typedef uint64_t VkDeviceAddress;

namespace voxen::gfx
{

// Controls streaming of chunk surface meshes and collects
// lists of draw commands according to render area and LODs.
class LandLoader {
public:
	// Information needed to draw geometry for one chunk.
	// TODO: should use graphics API abstraction types.
	struct DrawCommand {
		// Key of the chunk to draw
		land::ChunkKey chunk_key;

		// API handle of the index buffer storing 16-bit indices
		VkBuffer index_buffer;
		// First index in the buffer belonging to this mesh
		uint32_t first_index;
		// Number of indices in the buffer belonging to this mesh.
		// Primitive topology is the standard "triangle list".
		uint32_t num_indices;

		// GPU address of the first vertex position data item.
		// Data items are `land::PseudoSurfaceVertexPosition` and are tightly packed.
		VkDeviceAddress pos_data_address;
		// GPU address of the first vertex attributes data item.
		// Data items are `land::PseudoSurfaceVertexAttributes` and are tightly packed.
		VkDeviceAddress attrib_data_address;
	};

	using DrawList = std::vector<DrawCommand>;

	explicit LandLoader(GfxSystem &gfx, svc::ServiceLocator &svc);
	LandLoader(LandLoader &&) = delete;
	LandLoader(const LandLoader &) = delete;
	LandLoader &operator=(LandLoader &&) = delete;
	LandLoader &operator=(const LandLoader &) = delete;
	~LandLoader();

	void onNewState(const world::State &state);

	// Collects chunk surfaces within render area centered around `viewpoint`
	// according to LODs. Requests streaming of surfaces of those chunks
	// to VRAM and fills the list of draw commands for available surfaces.
	//
	// Commands in the list are not sorted in any particular order.
	//
	// Note: no frustum or any other kind of culling is performed.
	void makeDrawList(const glm::dvec3 &viewpoint, DrawList &dlist);

private:
	extras::pimpl<detail::LandLoaderImpl, 1024, alignof(void *)> m_impl;
};

} // namespace voxen::gfx
