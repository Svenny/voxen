#pragma once

#include <voxen/common/player.hpp>
#include <voxen/common/terrain.hpp>

#include <voxen/client/vulkan/vulkan_render.hpp>

#include <functional>

namespace voxen
{

class World {
public:
	World();
	World(const World &other);
	~World();

	const Player &player() const noexcept { return m_player; }

	double secondsPerTick() const noexcept { return 10.0 / 1000.0; }

	void update();

	void walkActiveChunks(std::function<void(const TerrainChunk &)> visitor) const;
	void render(VulkanRender &render);
private:
	Player m_player;
	TerrainOctree m_terrain;
};

}
