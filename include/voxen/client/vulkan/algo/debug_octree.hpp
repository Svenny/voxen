#pragma once

#include <voxen/client/vulkan/high/mesh.hpp>

#include <voxen/common/gameview.hpp>

namespace voxen::client::vulkan
{

class AlgoDebugOctree {
public:
	AlgoDebugOctree();

	void executePass(VkCommandBuffer cmd_buffer, const GameView &view);
private:
	Mesh m_cell_mesh;

	static Mesh createCellMesh();
};

}
