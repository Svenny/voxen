#pragma once

#include <voxen/common/player.hpp>
#include <voxen/common/terrain.hpp>

#include <voxen/client/vulkan/vulkan_render.hpp>

namespace voxen
{

class World {
public:
	World();
	~World();

	const Player &player() const noexcept { return m_player; }

	double secondsPerTick() const noexcept { return 10.0 / 1000.0; }

	void update();
	void render(VulkanRender &render);
private:
	Player m_player;
	TerrainOctree m_terrain;
};

}
