#pragma once

#include <voxen/common/terrain/qef_solver.hpp>
#include <voxen/common/terrain/types.hpp>

#include <array>

namespace voxen
{

struct TerrainChunkOctreeLeaf {
	bool is_leaf;
	uint8_t depth;

	glm::vec3 surface_vertex;
	glm::vec3 surface_normal;
	uint32_t surface_vertex_id;
	std::array<voxel_t, 8> corners;
	QefSolver3D::State qef_state;
};
static_assert(sizeof(TerrainChunkOctreeLeaf) == 96, "96-byte TerrainChunkOctreeLeaf packing is broken");

struct TerrainChunkOctreeNode {
	bool is_leaf;
	uint8_t depth;

	uint8_t children_leafmask;
	uint32_t children_ids[8];

	bool isChildLeaf(int num) const noexcept { return (children_leafmask & (1 << num)) != 0; }
};
static_assert(sizeof(TerrainChunkOctreeNode) == 36, "36-byte TerrainChunkOctreeNode packing is broken");

static_assert(offsetof(TerrainChunkOctreeLeaf, is_leaf) == offsetof(TerrainChunkOctreeNode, is_leaf),
              "Dynamic TerrainChunkOctree[Leaf|Node] typing is broken");

}
