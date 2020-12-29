#pragma once

#include <voxen/client/vulkan/high/mesh.hpp>

#include <voxen/common/world_state.hpp>
#include <voxen/common/gameview.hpp>

namespace voxen::client::vulkan
{

class AlgoTerrainSimple {
public:
	AlgoTerrainSimple();

	void executePass(VkCommandBuffer cmd_buffer, const WorldState &state, const GameView &view);

private:
	bool isChunkVisible(const TerrainChunk &chunk, const GameView &view) const noexcept;
};

}
