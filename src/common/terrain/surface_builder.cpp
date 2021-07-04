#include <voxen/common/terrain/surface_builder.hpp>

#include <voxen/common/terrain/chunk_octree.hpp>
#include <voxen/common/terrain/octree_tables.hpp>
#include <voxen/common/terrain/primary_data.hpp>
#include <voxen/common/terrain/surface.hpp>
#include <voxen/util/log.hpp>

#include <algorithm>
#include <array>
#include <vector>

namespace voxen::terrain
{

namespace
{

void addLeafToSurface(ChunkOctreeLeaf *leaf, ChunkOwnSurface &surface)
{
	assert(leaf);
	//TODO(sirgienko) add Material support, when we will have any voxel data
	//MaterialFilter filter;
	const glm::vec3 &vertex = leaf->surface_vertex;
	const glm::vec3 &normal = leaf->surface_normal;
	// TODO (Svenny): add material selection
	leaf->surface_vertex_id = surface.addVertex({ vertex, normal });
}

uint32_t addLeafToSurface(const ChunkOctreeLeaf *leaf, ChunkSeamSurface &surface, ChunkId base_id, ChunkId foreign_id)
{
	assert(leaf);
	//TODO(sirgienko) add Material support, when we will have any voxel data
	//MaterialFilter filter;

	// Apply position/scale correction for foreign vertex.
	// It's in chunk-local coordinates of another chunk, convert it to ours.
	glm::vec3 vertex = leaf->surface_vertex;

	// Correct for the scale
	if (base_id.lod < foreign_id.lod) {
		vertex *= float(1u << (foreign_id.lod - base_id.lod));
	} else if (base_id.lod > foreign_id.lod) {
		vertex /= float(1u << (base_id.lod - foreign_id.lod));
	}

	// Correct for the origin
	vertex += glm::vec3(foreign_id.base_x - base_id.base_x,
	                    foreign_id.base_y - base_id.base_y,
	                    foreign_id.base_z - base_id.base_z) * (float(Config::CHUNK_SIZE) / float(1u << base_id.lod));

	const glm::vec3 &normal = leaf->surface_normal;
	// TODO (Svenny): add material selection
	return surface.addExtraVertex({ vertex, normal });
}

using LeafToIdxMap = std::unordered_map<const ChunkOctreeLeaf *, uint32_t>;

uint32_t getForeignLeafVertexIndex(LeafToIdxMap &map, const ChunkOctreeLeaf *leaf, ChunkSeamSurface &surface, ChunkId base_id, ChunkId foreign_id)
{
	auto iter = map.find(leaf);
	if (iter != map.end()) {
		return iter->second;
	}

	uint32_t idx = addLeafToSurface(leaf, surface, base_id, foreign_id);
	map.emplace(leaf, idx);
	return idx;
}

struct EdgeProcOwnArgs final {
	std::array<const ChunkOctreeNodeBase *, 4> nodes;
	const ChunkOctree &octree;
	ChunkOwnSurface &surface;
};

struct EdgeProcSeamArgs final {
	std::array<const ChunkOctreeNodeBase *, 4> nodes;
	std::array<const ChunkOctree *, 4> octrees;
	std::array<ChunkId, 4> chunk_ids;
	ChunkSeamSurface &surface;
	LeafToIdxMap &foreign_leaf_to_idx;
};

template<bool S>
using EdgeProcArgs = std::conditional_t<S, EdgeProcSeamArgs, EdgeProcOwnArgs>;

template<int D, bool S>
void edgeProc(EdgeProcArgs<S> args)
{
	constexpr int CORNERS_TABLE[3][4][2] = {
		{ { 5, 7 }, { 1, 3 }, { 0, 2 }, { 4, 6 } }, // X
		{ { 3, 7 }, { 2, 6 }, { 0, 4 }, { 1, 5 } }, // Y
		{ { 6, 7 }, { 4, 5 }, { 0, 1 }, { 2, 3 } }  // Z
	};

	const std::array<const ChunkOctreeNodeBase *, 4> &nodes = args.nodes;

	if (!nodes[0] || !nodes[1] || !nodes[2] || !nodes[3]) {
		// If at least one node is missing then (by topological safety test guarantee) this
		// edge is not surface-crossing (otherwise there was an unsafe node collapse).
		return;
	}

	const ChunkOctreeNodeBase *sub[8];
	bool all_leaves = true;
	for (int i = 0; i < 8; i++) {
		const int node_id = EDGE_PROC_RECURSION_TABLE[D][i][0];
		const ChunkOctreeNodeBase *n = nodes[node_id];

		if (!n->is_leaf) {
			const ChunkOctreeCell *cell = n->castToCell();
			const uint32_t child_id = cell->children_ids[EDGE_PROC_RECURSION_TABLE[D][i][1]];

			if constexpr (S) {
				sub[i] = args.octrees[node_id]->idToPointer(child_id);
			} else {
				sub[i] = args.octree.idToPointer(child_id);
			}

			all_leaves = false;
		} else {
			sub[i] = n;
		}
	}

	if (!all_leaves) {
		for (int i = 0; i < 2; i++) {
			args.nodes[0] = sub[SUBEDGE_SHARING_TABLE[D][i][0]];
			args.nodes[1] = sub[SUBEDGE_SHARING_TABLE[D][i][1]];
			args.nodes[2] = sub[SUBEDGE_SHARING_TABLE[D][i][2]];
			args.nodes[3] = sub[SUBEDGE_SHARING_TABLE[D][i][3]];
			edgeProc<D, S>(args);
		}
		return;
	}

	// All four entries in `nodes` are leaves, guaranteed by above `if`
	const ChunkOctreeLeaf *leaves[4];
	for (int i = 0; i < 4; i++) {
		leaves[i] = nodes[i]->castToLeaf();
	}

	voxel_t mat1 = 0, mat2 = 0;
	/* Find the minimal node, i.e. the node with the maximal depth. By looking at its
	 materials on endpoints of this edge we may know whether the edge is surface-crossing
	 and if we need to flip the triangles winding order. */
	int8_t max_depth = 0;
	for (int i = 0; i < 4; i++) {
		if (leaves[i]->depth > max_depth) {
			max_depth = leaves[i]->depth;
			mat1 = leaves[i]->corners[CORNERS_TABLE[D][i][0]];
			mat2 = leaves[i]->corners[CORNERS_TABLE[D][i][1]];
		}
	}
	if (mat1 == mat2 || (mat1 != 0 && mat2 != 0)) {
		return; // Not a surface-crossing edge
	}

	/* We assume that lesser endpoint is solid. If this is not the case, the triangles'
	 winding order should be flipped to remain facing outside of the surface. */
	const bool flip = (mat1 == 0);

	/* Leaf #0 is guaranteed to belong to "our" chunk, otherwise the call topology was
	 invalid. Therefore we can classify others as "foreign" by comparing with it. */
	uint32_t id0 = leaves[0]->surface_vertex_id;
	uint32_t id1, id2, id3;
	if constexpr (S) {
		auto get_index = [&args](const ChunkOctreeLeaf *leaf, int id) {
			if (args.octrees[id] == args.octrees[0]) {
				// This is "our" leaf, use its own precalculated vertex ID
				return leaf->surface_vertex_id;
			}
			// This is "foreign" leaf
			return getForeignLeafVertexIndex(args.foreign_leaf_to_idx, leaf, args.surface, args.chunk_ids[0], args.chunk_ids[id]);
		};

		id1 = get_index(leaves[1], 1);
		id2 = get_index(leaves[2], 2);
		id3 = get_index(leaves[3], 3);
	} else {
		id1 = leaves[1]->surface_vertex_id;
		id2 = leaves[2]->surface_vertex_id;
		id3 = leaves[3]->surface_vertex_id;
	}

	if (!flip) {
		args.surface.addTriangle(id0, id1, id2);
		args.surface.addTriangle(id0, id2, id3);
	} else {
		args.surface.addTriangle(id0, id2, id1);
		args.surface.addTriangle(id0, id3, id2);
	}
}

struct FaceProcOwnArgs final {
	std::array<const ChunkOctreeNodeBase *, 2> nodes;
	const ChunkOctree &octree;
	ChunkOwnSurface &surface;

	EdgeProcOwnArgs toEdgeProcArgs() const noexcept
	{
		return EdgeProcOwnArgs {
			.octree = octree,
			.surface = surface
		};
	}
};

struct FaceProcSeamArgs final {
	std::array<const ChunkOctreeNodeBase *, 2> nodes;
	std::array<const ChunkOctree *, 2> octrees;
	std::array<ChunkId, 2> chunk_ids;
	ChunkSeamSurface &surface;
	LeafToIdxMap &foreign_leaf_to_idx;

	EdgeProcSeamArgs toEdgeProcArgs() const noexcept
	{
		return EdgeProcSeamArgs {
			.surface = surface,
			.foreign_leaf_to_idx = foreign_leaf_to_idx
		};
	}
};

template<bool S>
using FaceProcArgs = std::conditional_t<S, FaceProcSeamArgs, FaceProcOwnArgs>;

template<int D, bool S>
void faceProc(FaceProcArgs<S> args)
{
	const std::array<const ChunkOctreeNodeBase *, 2> &nodes = args.nodes;

	if (!nodes[0] || !nodes[1]) {
		// No valid lowest quadruples can be generated
		// from this pair if at least one node doesn't exist
		return;
	}

	const ChunkOctree *sub_octrees[8];
	ChunkId sub_ids[8];
	const ChunkOctreeNodeBase *sub[8];

	bool has_cells = false;
	for (int i = 0; i < 8; i++) {
		const int node_id = FACE_PROC_RECURSION_TABLE[D][i][0];
		const ChunkOctreeNodeBase *n = nodes[node_id];

		if constexpr (S) {
			sub_octrees[i] = args.octrees[node_id];
			sub_ids[i] = args.chunk_ids[node_id];
		}

		if (!n->is_leaf) {
			const ChunkOctreeCell *cell = n->castToCell();
			const uint32_t child_id = cell->children_ids[FACE_PROC_RECURSION_TABLE[D][i][1]];

			if constexpr (S) {
				sub[i] = args.octrees[node_id]->idToPointer(child_id);
			} else {
				sub[i] = args.octree.idToPointer(child_id);
			}

			has_cells = true;
		} else {
			sub[i] = n;
		}
	}

	if (!has_cells) {
		// Both nodes are leaves => no valid lowest octuples can be generated
		return;
	}

	for (int i = 0; i < 4; i++) {
		args.nodes[0] = sub[SUBFACE_SHARING_TABLE[D][i][0]];
		args.nodes[1] = sub[SUBFACE_SHARING_TABLE[D][i][1]];
		faceProc<D, S>(args);
	}

	EdgeProcArgs<S> edge_args = args.toEdgeProcArgs();

	constexpr int D1 = (D + 1) % 3;
	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 4; j++) {
			int idx = SUBEDGE_SHARING_TABLE[D1][i][j];
			edge_args.nodes[j] = sub[idx];
			if constexpr (S) {
				edge_args.octrees[j] = sub_octrees[idx];
				edge_args.chunk_ids[j] = sub_ids[idx];
			}
		}

		edgeProc<D1, S>(edge_args);
	}

	constexpr int D2 = (D + 2) % 3;
	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 4; j++) {
			int idx = SUBEDGE_SHARING_TABLE[D2][i][j];
			edge_args.nodes[j] = sub[idx];
			if constexpr (S) {
				edge_args.octrees[j] = sub_octrees[idx];
				edge_args.chunk_ids[j] = sub_ids[idx];
			}
		}

		edgeProc<D2, S>(edge_args);
	}
}

void cellProc(const ChunkOctreeNodeBase *node, const ChunkOctree &octree, ChunkOwnSurface &surface)
{
	if (!node) {
		return;
	}

	if (node->is_leaf) {
		// Nothing to do with leaves
		return;
	}
	const ChunkOctreeCell *cell = node->castToCell();

	const ChunkOctreeNodeBase *sub[8];
	for (int i = 0; i < 8; i++) {
		sub[i] = octree.idToPointer(cell->children_ids[i]);
		cellProc(sub[i], octree, surface);
	}

	FaceProcOwnArgs face_args {
		.octree = octree,
		.surface = surface
	};
	for (int i = 0; i < 4; i++) {
		face_args.nodes = { sub[SUBFACE_SHARING_TABLE[0][i][0]], sub[SUBFACE_SHARING_TABLE[0][i][1]] };
		faceProc<0, false>(face_args);

		face_args.nodes = { sub[SUBFACE_SHARING_TABLE[1][i][0]], sub[SUBFACE_SHARING_TABLE[1][i][1]] };
		faceProc<1, false>(face_args);

		face_args.nodes = { sub[SUBFACE_SHARING_TABLE[2][i][0]], sub[SUBFACE_SHARING_TABLE[2][i][1]] };
		faceProc<2, false>(face_args);
	}

	EdgeProcOwnArgs edge_args {
		.octree = octree,
		.surface = surface
	};
	for (int i = 0; i < 2; i++) {
		edge_args.nodes = { sub[SUBEDGE_SHARING_TABLE[0][i][0]], sub[SUBEDGE_SHARING_TABLE[0][i][1]],
		                    sub[SUBEDGE_SHARING_TABLE[0][i][2]], sub[SUBEDGE_SHARING_TABLE[0][i][3]] };
		edgeProc<0, false>(edge_args);

		edge_args.nodes = { sub[SUBEDGE_SHARING_TABLE[1][i][0]], sub[SUBEDGE_SHARING_TABLE[1][i][1]],
		                    sub[SUBEDGE_SHARING_TABLE[1][i][2]], sub[SUBEDGE_SHARING_TABLE[1][i][3]] };
		edgeProc<1, false>(edge_args);

		edge_args.nodes = { sub[SUBEDGE_SHARING_TABLE[2][i][0]], sub[SUBEDGE_SHARING_TABLE[2][i][1]],
		                    sub[SUBEDGE_SHARING_TABLE[2][i][2]], sub[SUBEDGE_SHARING_TABLE[2][i][3]] };
		edgeProc<2, false>(edge_args);
	}
}

void makeVertices(ChunkOctreeNodeBase *node, ChunkOctree &octree, ChunkOwnSurface &surface)
{
	if (!node) {
		return;
	}

	if (node->is_leaf) {
		addLeafToSurface(node->castToLeaf(), surface);
	} else {
		ChunkOctreeCell *cell = node->castToCell();
		for (int i = 0; i < 8; i++) {
			ChunkOctreeNodeBase *child = octree.idToPointer(cell->children_ids[i]);
			makeVertices(child, octree, surface);
		}
	}
}

const HermiteDataStorage &selectHermiteStorage(const ChunkPrimaryData &data, int dim) noexcept
{
	switch (dim) {
	case 0:
		return data.hermite_data_x;
	case 1:
		return data.hermite_data_y;
	case 2:
		return data.hermite_data_z;
	default:
		__builtin_unreachable();
	}
}

struct DcBuildArgs {
	ChunkOctree &octree;
	const ChunkPrimaryData &primary_data;
	QefSolver3D &solver;
	const float epsilon;
};

std::pair<uint32_t, ChunkOctreeLeaf *> buildLeaf(glm::ivec3 min_corner, int32_t size, int8_t depth, DcBuildArgs &args)
{
	QefSolver3D &solver = args.solver;

	solver.reset();
	glm::vec3 avg_normal { 0 };

	const auto &grid = args.primary_data.voxel_grid;
	std::array<voxel_t, 8> corners = grid.getCellLinear(min_corner.x, min_corner.y, min_corner.z);

	bool has_edges = false;

	constexpr int edge_table[3][4][2] = {
		{ { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 } }, // X
		{ { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } }, // Y
		{ { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 } }  // Z
	};
	for (int dim = 0; dim <= 2; dim++) {
		auto &storage = selectHermiteStorage(args.primary_data, dim);

		for (int i = 0; i < 4; i++) {
			voxel_t mat1 = corners[edge_table[dim][i][0]];
			voxel_t mat2 = corners[edge_table[dim][i][1]];
			if (mat1 == mat2)
				continue;
			if (mat1 != 0 && mat2 != 0)
				continue;

			has_edges = true;
			auto edge_pos = min_corner + CELL_CORNER_OFFSET_TABLE[edge_table[dim][i][0]];
			auto iter = storage.find(edge_pos.x, edge_pos.y, edge_pos.z);
			assert(iter != storage.end());

			glm::vec3 vertex = iter->surfacePoint();
			glm::vec3 normal = iter->surfaceNormal();
			solver.addPlane(vertex, normal);
			avg_normal += normal;
		}
	}

	if (!has_edges) {
		// No need to generate empty leaf
		return { ChunkOctree::INVALID_NODE_ID, nullptr };
	}

	auto[id, leaf] = args.octree.allocLeaf(depth);
	leaf->surface_normal = glm::normalize(avg_normal);
	leaf->corners = corners;

	glm::vec3 lower_bound(min_corner);
	glm::vec3 upper_bound(min_corner + size);
	leaf->surface_vertex = solver.solve(lower_bound, upper_bound);
	leaf->qef_state = solver.state();

	return { id, leaf };
}

using CubeMaterials = std::array<std::array<std::array<voxel_t, 3>, 3>, 3>;

// Check topological safety of collapsing 2x2x2 leaves to a single leaf.
// Checking by 3x3x3 cube of their corners' materials.
bool checkTopoSafety(const CubeMaterials &mats) noexcept
{
	// Maps DC cell vertices ordering to MC one
	constexpr int DC_TO_MC[8] = { 0, 3, 1, 2, 4, 7, 5, 6 };
	// Indicates whether a given vertex sign configuration
	// (in MC ordering) is manifold (i.e. non-ambiguous)
	constexpr bool IS_MANIFOLD[256] = {
		true, true, true, true, true, false, true, true, true, true, false, true, true, true, true, true,
		true, true, false, true, false, false, false, true, false, true, false, true, false, true, false, true,
		true, false, true, true, false, false, true, true, false, false, false, true, false, false, true, true,
		true, true, true, true, false, false, true, true, false, true, false, true, false, false, false, true,
		true, false, false, false, true, false, true, true, false, false, false, false, true, true, true, true,
		false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
		true, false, true, true, true, false, true, true, false, false, false, false, true, false, true, true,
		true, true, true, true, true, false, true, true, false, false, false, false, false, false, false, true,
		true, false, false, false, false, false, false, false, true, true, false, true, true, true, true, true,
		true, true, false, true, false, false, false, false, true, true, false, true, true, true, false, true,
		false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
		true, true, true, true, false, false, false, false, true, true, false, true, false, false, false, true,
		true, false, false, false, true, false, true, false, true, true, false, false, true, true, true, true,
		true, true, false, false, true, false, false, false, true, true, false, false, true, true, false, true,
		true, false, true, false, true, false, true, false, true, false, false, false, true, false, true, true,
		true, true, true, true, true, false, true, true, true, true, false, true, true, true, true, true,
	};

	// Construct corners sign mask in MC vertex ordering
	uint32_t mask = 0;
	for (int i = 0; i < 8; i++) {
		auto pos = 2 * CELL_CORNER_OFFSET_TABLE[i];
		voxel_t mat = mats[pos.y][pos.x][pos.z];
		if (mat != 0)
			mask |= uint32_t(1 << DC_TO_MC[i]);
	}
	if (!IS_MANIFOLD[mask]) {
		// Collapsing nodes will yield an ambiguous sign configuration
		return false;
	}

	// Now check ambiguity of all to-be-collapsed nodes
	for (int i = 0; i < 8; i++) {
		uint32_t submask = 0;
		// Same as with corners check above
		for (int j = 0; j < 8; j++) {
			auto pos = CELL_CORNER_OFFSET_TABLE[i] + CELL_CORNER_OFFSET_TABLE[j];
			voxel_t mat = mats[pos.y][pos.x][pos.z];
			if (mat != 0)
				submask |= uint32_t(1 << DC_TO_MC[j]);
		}
		if (!IS_MANIFOLD[submask]) {
			// Node already has ambiguous configuration
			return false;
		}
	}

	// Check edge midpoint signs (they must be equal to either edge endpoint)
	for (int c1 = 0; c1 < 3; c1++) {
		for (int c2 = 0; c2 < 3; c2++) {
			if (mats[1][c1][c2] != mats[0][c1][c2] && mats[1][c1][c2] != mats[2][c1][c2])
				return false;
			if (mats[c1][1][c2] != mats[c1][0][c2] && mats[c1][1][c2] != mats[c1][2][c2])
				return false;
			if (mats[c1][c2][1] != mats[c1][c2][0] && mats[c1][c2][1] != mats[c1][c2][2])
				return false;
		}
	}

	// Check face midpoint signs (they must be equal to either face corner)
	for (int c1 = 0; c1 < 3; c1++) {
		voxel_t mat;
		mat = mats[1][1][c1];
		if (mat != mats[0][0][c1] && mat != mats[0][2][c1] && mat != mats[2][0][c1] && mat != mats[2][2][c1])
			return false;
		mat = mats[1][c1][1];
		if (mat != mats[0][c1][0] && mat != mats[0][c1][2] && mat != mats[2][c1][0] && mat != mats[2][c1][2])
			return false;
		mat = mats[c1][1][1];
		if (mat != mats[c1][0][0] && mat != mats[c1][0][2] && mat != mats[c1][2][0] && mat != mats[c1][2][2])
			return false;
	}

	// Check cube midpoint sign (it must be equal to either cube corner)
	voxel_t mat = mats[1][1][1];
	for (int i = 0; i < 8; i++) {
		auto pos = 2 * CELL_CORNER_OFFSET_TABLE[i];
		if (mat == mats[pos.y][pos.x][pos.z]) {
			// All checks passed, this topology is safe to collapse
			return true;
		}
	}

	// Cube midpoint sign check failed
	return false;
}

std::pair<uint32_t, ChunkOctreeNodeBase *>buildNode(glm::ivec3 min_corner, int32_t size,
                                                    int8_t depth, DcBuildArgs &args)
{
	assert(size > 0);
	if (size == 1) {
		return buildLeaf(min_corner, size, depth, args);
	}

	uint32_t children_ids[8];
	bool has_children = false;
	bool has_child_cell = false;

	const int32_t child_size = size / 2;
	for (int i = 0; i < 8; i++) {
		glm::ivec3 child_min_corner = min_corner + child_size * CELL_CORNER_OFFSET_TABLE[i];
		auto[child_id, child_ptr] = buildNode(child_min_corner, child_size, depth + 1, args);

		children_ids[i] = child_id;

		if (child_ptr) {
			has_children = true;
			if (!child_ptr->is_leaf) {
				has_child_cell = true;
			}
		}
	}

	if (!has_children) {
		// No children - no need to create any node at all
		return { ChunkOctree::INVALID_NODE_ID, nullptr };
	}

	// This lambda is to be called when we can't
	// collapse childrens and make this cell a leaf
	auto makeCell = [&]() -> std::pair<uint32_t, ChunkOctreeNodeBase *> {
		// This node becomes a cell
		auto[id, cell] = args.octree.allocCell(depth);
		std::copy_n(children_ids, 8, cell->children_ids);
		return { id, cell };
	};

	if (has_child_cell) {
		// We can't do any simplification if there is a least one non-leaf child
		return makeCell();
	}

	// We cannot obtain pointers in the above loop as we're calling
	// `buildNode` at the same time which can invalidate pointers
	ChunkOctreeNodeBase *children_ptrs[8];
	for (int i = 0; i < 8; i++) {
		children_ptrs[i] = args.octree.idToPointer(children_ids[i]);
	}

	// All children which are present are guaranteed to be leaves.
	// Now we may collect materials for topological safety check.
	CubeMaterials mats;

	// Fill materials with that of a center by default. This is needed
	// to prevent undefined values when some children are absent. And
	// using center material is the safest way for topological check.
	voxel_t center_mat = 0;
	// At least one leaf is guaranteed to be found
	for (int i = 0; i < 8; i++) {
		if (children_ptrs[i]) {
			ChunkOctreeLeaf *leaf = children_ptrs[i]->castToLeaf();
			// `i` is a bitmask of per-axis offsets, i.e. 5 -> 101 means offset in Y and Z axes.
			// `i ^ 7` is an inverse mask, i.e. 5 ^ 7 = 2 -> 010 means offset in X axis.
			// `i`-th children's `i ^ 7`-th corner will always be in the center of parent cell.
			center_mat = leaf->corners[i ^ 7];
			break;
		}
	}
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			mats[i][j].fill(center_mat);
		}
	}

	// Now fill the rest
	for (int i = 0; i < 8; i++) {
		if (!children_ptrs[i]) {
			continue;
		}
		ChunkOctreeLeaf *leaf = children_ptrs[i]->castToLeaf();
		for (int j = 0; j < 8; j++) {
			auto offset = CELL_CORNER_OFFSET_TABLE[i] + CELL_CORNER_OFFSET_TABLE[j];
			mats[offset.y][offset.x][offset.z] = leaf->corners[j];
		}
	}
	if (!checkTopoSafety(mats)) {
		// Collapsing may change surface topology
		return makeCell();
	}

	// Extract corners from full cube materials. This will
	// be needed if we finally decide to collapse children.
	std::array<voxel_t, 8> corners;
	for (int i = 0; i < 8; i++) {
		auto offset = 2 * CELL_CORNER_OFFSET_TABLE[i];
		corners[i] = mats[offset.y][offset.x][offset.z];
	}

	auto &solver = args.solver;

	// Now try to combine childrens' QEF's
	solver.reset();
	glm::vec3 avg_normal { 0 };
	for (int i = 0; i < 8; i++) {
		if (!children_ptrs[i]) {
			continue;
		}
		ChunkOctreeLeaf *leaf = children_ptrs[i]->castToLeaf();
		avg_normal += leaf->surface_normal;
		solver.merge(leaf->qef_state);
	}

	glm::vec3 lower_bound(min_corner);
	glm::vec3 upper_bound(min_corner + size);
	glm::vec3 surface_vertex = solver.solve(lower_bound, upper_bound);
	float error = solver.eval(surface_vertex);
	if (error > args.epsilon) {
		// Error threshold is violated, collapsing
		// may severely degrade visual quality
		return makeCell();
	}

	// All collapse safety checks passed, this node is now definitely leaf

	// Free children which are not needed anymore
	for (int i = 0; i < 8; i++) {
		args.octree.freeNode(children_ids[i]);
	}

	auto[id, leaf] = args.octree.allocLeaf(depth);
	leaf->surface_vertex = surface_vertex;
	leaf->surface_normal = glm::normalize(avg_normal);
	leaf->corners = corners;
	leaf->qef_state = solver.state();

	return { id, leaf };
}

struct RootEqualizationArgs final {
	const ChunkOctree &octree;
	uint32_t descend_mask_x;
	uint32_t descend_mask_y;
	uint32_t descend_mask_z;
	uint32_t descend_length;
};

uint32_t getEqualizedRoot(RootEqualizationArgs args) noexcept
{
	uint32_t node_id = args.octree.baseRoot();

	// Go from the highest-order bits
	for (uint32_t i = args.descend_length - 1; ~i; i--) {
		const ChunkOctreeNodeBase *node = args.octree.idToPointer(node_id);
		if (!node || node->is_leaf) {
			// We've hit a non-existent node or leaf before completing the descend
			return node_id;
		}

		const uint32_t bit = (1u << i);
		uint32_t child_id = 0;
		child_id |= (args.descend_mask_x & bit) ? 2u : 0u;
		child_id |= (args.descend_mask_y & bit) ? 4u : 0u;
		child_id |= (args.descend_mask_z & bit) ? 1u : 0u;

		node_id = node->castToCell()->children_ids[child_id];
	}

	return node_id;
}

template<int D>
void doBuildFaceSeam(Chunk &my, const Chunk &his, LeafToIdxMap &foreign_leaf_to_idx)
{
	const ChunkId my_id = my.id();
	const ChunkId his_id = his.id();

	ChunkOctree &my_octree = my.octree();
	const ChunkOctree &his_octree = his.octree();

	uint32_t my_root_id = my_octree.baseRoot();
	uint32_t his_root_id = his_octree.baseRoot();

	if (my_id.lod < his_id.lod) {
		// "My" chunk is smaller, need to equalize "his" root
		uint32_t descend_mask[3];
		descend_mask[0] = uint32_t(my_id.base_x - his_id.base_x);
		descend_mask[1] = uint32_t(my_id.base_y - his_id.base_y);
		descend_mask[2] = uint32_t(my_id.base_z - his_id.base_z);
		descend_mask[D] = 0u; // Always take children with smaller `D` coord

		his_root_id = getEqualizedRoot(RootEqualizationArgs {
			.octree = his_octree,
			.descend_mask_x = descend_mask[0],
			.descend_mask_y = descend_mask[1],
			.descend_mask_z = descend_mask[2],
			.descend_length = his_id.lod - my_id.lod
		});
	} else if (my_id.lod > his_id.lod) {
		// "his" chunk is smaller, need to equalize "my" root
		uint32_t descend_mask[3];
		descend_mask[0] = uint32_t(his_id.base_x - my_id.base_x);
		descend_mask[1] = uint32_t(his_id.base_y - my_id.base_y);
		descend_mask[2] = uint32_t(his_id.base_z - my_id.base_z);
		descend_mask[D] = ~0u; // Always take children with bigger `D` coord

		my_root_id = getEqualizedRoot(RootEqualizationArgs {
			.octree = my_octree,
			.descend_mask_x = descend_mask[0],
			.descend_mask_y = descend_mask[1],
			.descend_mask_z = descend_mask[2],
			.descend_length = my_id.lod - his_id.lod
		});
	}

	const ChunkOctreeNodeBase *my_root = my_octree.idToPointer(my_root_id);
	const ChunkOctreeNodeBase *his_root = his_octree.idToPointer(his_root_id);

	faceProc<D, true>(FaceProcSeamArgs {
		.nodes = { my_root, his_root },
		.octrees = { &my_octree, &his_octree },
		.chunk_ids = { my_id, his_id },
		.surface = my.seamSurface(),
		.foreign_leaf_to_idx = foreign_leaf_to_idx
	});
}

template<int D>
void doBuildEdgeSeam(Chunk &my, const Chunk &his_a, const Chunk &his_ab,
                     const Chunk &his_b, LeafToIdxMap &foreign_leaf_to_idx)
{
	std::array<const Chunk *, 4> chunks = { &my, &his_a, &his_ab, &his_b };
	std::array<ChunkId, 4> ids;
	std::array<const ChunkOctree *, 4> octrees;

	ChunkId min_chunk_id { .lod = UINT32_MAX };

	for (int i = 0; i < 4; i++) {
		ids[i] = chunks[i]->id();
		octrees[i] = &chunks[i]->octree();

		if (ids[i].lod < min_chunk_id.lod) {
			min_chunk_id = ids[i];
		}
	}

	constexpr int D1 = (D + 1) % 3;
	constexpr int D2 = (D + 2) % 3;

	std::array<const ChunkOctreeNodeBase *, 4> roots;
	for (int i = 0; i < 4; i++) {
		uint32_t root_id = octrees[i]->baseRoot();

		if (ids[i].lod > min_chunk_id.lod) {
			// "his" chunk is smaller, need to equalize "my" root
			uint32_t descend_mask[3];
			descend_mask[0] = uint32_t(min_chunk_id.base_x - ids[i].base_x);
			descend_mask[1] = uint32_t(min_chunk_id.base_y - ids[i].base_y);
			descend_mask[2] = uint32_t(min_chunk_id.base_z - ids[i].base_z);
			descend_mask[D1] = (i == 0 || i == 3) ? ~0u : 0u;
			descend_mask[D2] = (i == 0 || i == 1) ? ~0u : 0u;

			// This chunk is bigger than minimal, need to equalize its root
			root_id = getEqualizedRoot(RootEqualizationArgs {
				.octree = *octrees[i],
				.descend_mask_x = descend_mask[0],
				.descend_mask_y = descend_mask[1],
				.descend_mask_z = descend_mask[2],
				.descend_length = ids[i].lod - min_chunk_id.lod
			});
		}

		roots[i] = octrees[i]->idToPointer(root_id);
	}

	edgeProc<D, true>(EdgeProcSeamArgs {
		.nodes = roots,
		.octrees = octrees,
		.chunk_ids = ids,
		.surface = my.seamSurface(),
		.foreign_leaf_to_idx = foreign_leaf_to_idx
	});
}

} // end anonymous namespace

void SurfaceBuilder::buildOctree()
{
	ChunkOctree &octree = m_chunk.octree();
	octree.clear();

	QefSolver3D qef_solver;
	DcBuildArgs args {
		.octree = octree,
		.primary_data = m_chunk.primaryData(),
		.solver = qef_solver,
		.epsilon = 0.12f
	};

	auto[root_id, root] = buildNode(glm::ivec3(0), Config::CHUNK_SIZE, 0, args);
	octree.setBaseRoot(root_id);
}

void SurfaceBuilder::buildOwnSurface()
{
	ChunkOctree &octree = m_chunk.octree();
	ChunkOwnSurface &surface = m_chunk.ownSurface();
	surface.clear();

	auto *root = octree.idToPointer(octree.baseRoot());
	makeVertices(root, octree, surface);
	cellProc(root, octree, surface);
}

template<> void SurfaceBuilder::buildFaceSeam<0>(const Chunk &other)
{
	doBuildFaceSeam<0>(m_chunk, other, m_foreign_leaf_to_idx);
}

template<> void SurfaceBuilder::buildFaceSeam<1>(const Chunk &other)
{
	doBuildFaceSeam<0>(m_chunk, other, m_foreign_leaf_to_idx);
}

template<> void SurfaceBuilder::buildFaceSeam<2>(const Chunk &other)
{
	doBuildFaceSeam<2>(m_chunk, other, m_foreign_leaf_to_idx);
}

template<> void SurfaceBuilder::buildEdgeSeam<0>(const Chunk &other_y, const Chunk &other_yz, const Chunk &other_z)
{
	doBuildEdgeSeam<0>(m_chunk, other_y, other_yz, other_z, m_foreign_leaf_to_idx);
}

template<> void SurfaceBuilder::buildEdgeSeam<1>(const Chunk &other_z, const Chunk &other_xz, const Chunk &other_x)
{
	doBuildEdgeSeam<1>(m_chunk, other_z, other_xz, other_x, m_foreign_leaf_to_idx);
}

template<> void SurfaceBuilder::buildEdgeSeam<2>(const Chunk &other_x, const Chunk &other_xy, const Chunk &other_y)
{
	doBuildEdgeSeam<2>(m_chunk, other_x, other_xy, other_y, m_foreign_leaf_to_idx);
}

}
