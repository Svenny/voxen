#pragma once

#include <voxen/client/vulkan/common.hpp>

#include <voxen/common/world_state.hpp>
#include <voxen/common/gameview.hpp>

namespace voxen::client::vulkan
{

class AlgoTerrainSimple {
public:
	AlgoTerrainSimple();
	AlgoTerrainSimple(AlgoTerrainSimple &&) = delete;
	AlgoTerrainSimple(const AlgoTerrainSimple &) = delete;
	AlgoTerrainSimple &operator = (AlgoTerrainSimple &&) = delete;
	AlgoTerrainSimple &operator = (const AlgoTerrainSimple &) = delete;
	~AlgoTerrainSimple() = default;

	void executePass(VkCommandBuffer cmd_buffer, const WorldState &state, const GameView &view);

private:
	bool isChunkVisible(const TerrainChunk &chunk, const GameView &view) const noexcept;
};

}
