#include <voxen/common/terrain/surface_builder.hpp>

#include <voxen/common/terrain/octree_tables.hpp>
#include <voxen/util/log.hpp>

#include <algorithm>
#include <array>
#include <vector>

namespace voxen
{

template<int D>
static void edgeProc(std::array<const ChunkOctreeNodeBase *, 4> nodes, const ChunkOctree &octree, TerrainSurface &surface)
{
	constexpr int CORNERS_TABLE[3][4][2] = {
		{ { 5, 7 }, { 1, 3 }, { 0, 2 }, { 4, 6 } }, // X
		{ { 3, 7 }, { 2, 6 }, { 0, 4 }, { 1, 5 } }, // Y
		{ { 6, 7 }, { 4, 5 }, { 0, 1 }, { 2, 3 } }  // Z
	};

	if (!nodes[0] || !nodes[1] || !nodes[2] || !nodes[3]) {
		// If at least one node is missing then (by topological safety test guarantee) this
		// edge is not surface-crossing (otherwise there was an unsafe node collapse).
		return;
	}

	const ChunkOctreeNodeBase *sub[8];
	bool all_leaves = true;
	for (int i = 0; i < 8; i++) {
		const ChunkOctreeNodeBase *n = nodes[EDGE_PROC_RECURSION_TABLE[D][i][0]];
		if (!n->is_leaf) {
			const ChunkOctreeCell *cell = n->castToCell();
			sub[i] = octree.idToPointer(cell->children_ids[EDGE_PROC_RECURSION_TABLE[D][i][1]]);
			all_leaves = false;
		}
		else sub[i] = n;
	}
	if (!all_leaves) {
		for (int i = 0; i < 2; i++) {
			int i1 = SUBEDGE_SHARING_TABLE[D][i][0];
			int i2 = SUBEDGE_SHARING_TABLE[D][i][1];
			int i3 = SUBEDGE_SHARING_TABLE[D][i][2];
			int i4 = SUBEDGE_SHARING_TABLE[D][i][3];
			edgeProc<D>({ sub[i1], sub[i2], sub[i3], sub[i4] }, octree, surface);
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
	uint8_t max_depth = 0;
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
	bool flip = (mat1 == 0);
	uint32_t id0 = leaves[0]->surface_vertex_id;
	uint32_t id1 = leaves[1]->surface_vertex_id;
	uint32_t id2 = leaves[2]->surface_vertex_id;
	uint32_t id3 = leaves[3]->surface_vertex_id;
	if (!flip) {
		surface.addTriangle(id0, id1, id2);
		surface.addTriangle(id0, id2, id3);
	} else {
		surface.addTriangle(id0, id2, id1);
		surface.addTriangle(id0, id3, id2);
	}
}

template<int D>
static void faceProc(std::array<const ChunkOctreeNodeBase *, 2> nodes, const ChunkOctree &octree, TerrainSurface &surface)
{
	if (!nodes[0] || !nodes[1]) {
		// No valid lowest quadruples can be generated
		// from this pair if at least one node doesn't exist
		return;
	}

	const ChunkOctreeNodeBase *sub[8];
	bool has_cells = false;
	for (int i = 0; i < 8; i++) {
		const ChunkOctreeNodeBase *n = nodes[FACE_PROC_RECURSION_TABLE[D][i][0]];
		if (!n->is_leaf) {
			const ChunkOctreeCell *cell = n->castToCell();
			sub[i] = octree.idToPointer(cell->children_ids[FACE_PROC_RECURSION_TABLE[D][i][1]]);
			has_cells = true;
		}
		else sub[i] = n;
	}
	if (!has_cells) {
		// Both nodes are leaves => no valid lowest octuples can be generated
		return;
	}

	for (int i = 0; i < 4; i++) {
		int i1 = SUBFACE_SHARING_TABLE[D][i][0];
		int i2 = SUBFACE_SHARING_TABLE[D][i][1];
		faceProc<D>({ sub[i1], sub[i2] }, octree, surface);
	}
	constexpr int D1 = (D + 1) % 3;
	for (int i = 0; i < 2; i++) {
		int i1 = SUBEDGE_SHARING_TABLE[D1][i][0];
		int i2 = SUBEDGE_SHARING_TABLE[D1][i][1];
		int i3 = SUBEDGE_SHARING_TABLE[D1][i][2];
		int i4 = SUBEDGE_SHARING_TABLE[D1][i][3];
		edgeProc<D1>({ sub[i1], sub[i2], sub[i3], sub[i4] }, octree, surface);
	}
	constexpr int D2 = (D + 2) % 3;
	for (int i = 0; i < 2; i++) {
		int i1 = SUBEDGE_SHARING_TABLE[D2][i][0];
		int i2 = SUBEDGE_SHARING_TABLE[D2][i][1];
		int i3 = SUBEDGE_SHARING_TABLE[D2][i][2];
		int i4 = SUBEDGE_SHARING_TABLE[D2][i][3];
		edgeProc<D2>({ sub[i1], sub[i2], sub[i3], sub[i4] }, octree, surface);
	}
}

static void cellProc(const ChunkOctreeNodeBase *node, const ChunkOctree &octree, TerrainSurface &surface)
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
	for (int i = 0; i < 4; i++) {
		faceProc<0>({ sub[SUBFACE_SHARING_TABLE[0][i][0]], sub[SUBFACE_SHARING_TABLE[0][i][1]] }, octree, surface);
		faceProc<1>({ sub[SUBFACE_SHARING_TABLE[1][i][0]], sub[SUBFACE_SHARING_TABLE[1][i][1]] }, octree, surface);
		faceProc<2>({ sub[SUBFACE_SHARING_TABLE[2][i][0]], sub[SUBFACE_SHARING_TABLE[2][i][1]] }, octree, surface);
	}
	for (int i = 0; i < 2; i++) {
		edgeProc<0>({ sub[SUBEDGE_SHARING_TABLE[0][i][0]], sub[SUBEDGE_SHARING_TABLE[0][i][1]],
		              sub[SUBEDGE_SHARING_TABLE[0][i][2]], sub[SUBEDGE_SHARING_TABLE[0][i][3]] }, octree, surface);
		edgeProc<1>({ sub[SUBEDGE_SHARING_TABLE[1][i][0]], sub[SUBEDGE_SHARING_TABLE[1][i][1]],
		              sub[SUBEDGE_SHARING_TABLE[1][i][2]], sub[SUBEDGE_SHARING_TABLE[1][i][3]] }, octree, surface);
		edgeProc<2>({ sub[SUBEDGE_SHARING_TABLE[2][i][0]], sub[SUBEDGE_SHARING_TABLE[2][i][1]],
		              sub[SUBEDGE_SHARING_TABLE[2][i][2]], sub[SUBEDGE_SHARING_TABLE[2][i][3]] }, octree, surface);
	}
}

static void makeVertices(ChunkOctreeNodeBase *node, ChunkOctree &octree, TerrainSurface &surface)
{
	if (!node) {
		return;
	}
	//TODO(sirgienko) add Material support, when we will have any voxel data
	//MaterialFilter filter;
	if (node->is_leaf) {
		ChunkOctreeLeaf *leaf = node->castToLeaf();
		const glm::vec3 &vertex = leaf->surface_vertex;
		const glm::vec3 &normal = leaf->surface_normal;
		// TODO (Svenny): add material selection
		leaf->surface_vertex_id = surface.addVertex({ vertex, normal });
	} else {
		ChunkOctreeCell *cell = node->castToCell();
		for (int i = 0; i < 8; i++) {
			ChunkOctreeNodeBase *child = octree.idToPointer(cell->children_ids[i]);
			makeVertices(child, octree, surface);
		}
	}
}

struct DcBuildArgs {
	ChunkOctree &octree;
	const TerrainChunkPrimaryData &primary_data;
	QefSolver3D &solver;
	const float epsilon;
};

static const HermiteDataStorage &selectHermiteStorage(const TerrainChunkPrimaryData &grid, int dim) noexcept
{
	switch (dim) {
	case 0:
		return grid.hermite_data_x;
	case 1:
		return grid.hermite_data_y;
	case 2:
		return grid.hermite_data_z;
	default:
		__builtin_unreachable();
	}
}

static std::pair<uint32_t, ChunkOctreeLeaf *>
	buildLeaf(glm::ivec3 min_corner, int32_t size, uint8_t depth, DcBuildArgs &args)
{
	QefSolver3D &solver = args.solver;
	const auto &grid = args.primary_data;

	solver.reset();
	glm::vec3 avg_normal { 0 };

	std::array<voxel_t, 8> corners = grid.materialsOfCell(min_corner);

	bool has_edges = false;

	constexpr int edge_table[3][4][2] = {
		{ { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 } }, // X
		{ { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } }, // Y
		{ { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 } }  // Z
	};
	for (int dim = 0; dim <= 2; dim++) {
		auto &storage = selectHermiteStorage(grid, dim);

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
static bool checkTopoSafety(const CubeMaterials &mats) noexcept
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

static std::pair<uint32_t, ChunkOctreeNodeBase *>
	buildNode(glm::ivec3 min_corner, int32_t size, uint8_t depth, DcBuildArgs &args)
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

void TerrainSurfaceBuilder::calcSurface(const TerrainChunkPrimaryData &input, TerrainChunkSecondaryData &output)
{
	auto &octree = output.octree;
	auto &surface = output.surface;

	octree.clear();
	surface.clear();

	QefSolver3D qef_solver;
	DcBuildArgs args {
		.octree = octree,
		.primary_data = input,
		.solver = qef_solver,
		.epsilon = 0.01f
	};

	auto[root_id, root] = buildNode(glm::ivec3(0), TerrainChunkPrimaryData::GRID_CELL_COUNT, 0, args);
	if (!root) {
		// Simplified to nothing?
		return;
	}

	octree.setRoot(root_id);
	makeVertices(root, octree, surface);
	cellProc(root, octree, surface);
}

}
