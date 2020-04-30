#include <voxen/common/world.hpp>
#include <voxen/util/log.hpp>

namespace voxen
{

World::World() : m_terrain(/*1<<21*/16, /*1 << 12*/4) {
}

World::~World() {}

void World::update() {
	auto pos = m_player.position();
	m_terrain.updateChunks(pos.x, pos.y, pos.z);
}

void World::render(VulkanRender &render) {
	m_terrain.walk([&](int64_t x, int64_t y, int64_t z, int64_t sz) {
		render.debugDrawOctreeNode(m_player, float(x), float(y), float(z), float(sz));
	});
}

}
