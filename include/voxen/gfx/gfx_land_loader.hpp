#pragma once

#include <voxen/gfx/gfx_fwd.hpp>
#include <voxen/svc/svc_fwd.hpp>

#include <extras/pimpl.hpp>

#include <glm/vec3.hpp>

#include <memory>
#include <vector>

typedef struct VkBuffer_T *VkBuffer;
typedef uint64_t VkDeviceAddress;

namespace voxen
{

class WorldState;

} // namespace voxen

namespace voxen::gfx
{

namespace detail
{

class LandLoaderImpl;

}

class LandLoader {
public:
	constexpr static uint32_t FAKE_FACE_BATCH_SIZE = 256;

	struct DrawCommand {
		VkBuffer index_buffer;
		uint32_t first_index;
		uint32_t num_indices;

		VkDeviceAddress pos_data_address;
		VkDeviceAddress attrib_data_address;

		int32_t chunk_base_x;
		int32_t chunk_base_y;
		int32_t chunk_base_z;
		uint32_t chunk_lod;
	};

	using DrawList = std::vector<DrawCommand>;

	explicit LandLoader(GfxSystem &gfx, svc::ServiceLocator &svc);
	~LandLoader() noexcept;

	void onNewState(const WorldState &state);

	void makeDrawList(const glm::dvec3 &viewpoint, DrawList &dlist);

private:
	extras::pimpl<detail::LandLoaderImpl, 2048, alignof(void *)> m_impl;
};

} // namespace voxen::gfx
