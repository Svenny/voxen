#pragma once

#include <extras/pimpl.hpp>

#include <glm/vec3.hpp>

#include <memory>
#include <vector>

typedef struct VkBuffer_T *VkBuffer;

namespace voxen
{

class WorldState;

}

namespace voxen::gfx
{

namespace vk
{

class AsyncDma;
class Device;

}

class LandLoader {
public:
	constexpr static size_t FAKE_FACE_BATCH_SIZE = 256;

	struct RenderCmd {
		int32_t chunk_base_x;
		int32_t chunk_base_y;
		int32_t chunk_base_z;

		uint32_t face_mask : 4;
		uint32_t chunk_lod : 8;
		uint32_t num_faces : 20;

		uint32_t data_pool_id : 16;
		uint32_t data_pool_slot : 16;
	};
	static_assert(sizeof(RenderCmd) == 20);

	using RenderList = std::vector<RenderCmd>;

	explicit LandLoader(vk::Device &dev);
	~LandLoader() noexcept;

	void onNewState(std::shared_ptr<const WorldState> state, vk::AsyncDma &dma);
	void onNewFrame();

	void makeRenderList(const glm::dvec3 &viewpoint, RenderList &rlist);

	VkBuffer getFakeDataPool(uint32_t pool);

private:
	struct Impl;

	extras::pimpl<Impl, 1024, alignof(void *)> m_impl;
};

} // namespace voxen::gfx
