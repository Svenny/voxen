#include <voxen/common/terrain/surface_builder.hpp>

#include <voxen/common/terrain/chunk.hpp>
#include <voxen/common/terrain/chunk_octree.hpp>
#include <voxen/common/terrain/octree_tables.hpp>
#include <voxen/common/terrain/primary_data.hpp>
#include <voxen/common/terrain/surface.hpp>
#include <voxen/util/log.hpp>

#include <algorithm>
#include <array>
#include <unordered_map>
#include <vector>

namespace voxen::terrain
{

namespace
{

void addLeafToSurface(ChunkOctreeLeaf *leaf, ChunkSurface &surface)
{
	assert(leaf);
	//TODO(sirgienko) add Material support, when we will have any voxel data
	//MaterialFilter filter;
	const glm::vec3 &vertex = leaf->surface_vertex;
	const glm::vec3 &normal = leaf->surface_normal;
	// TODO (Svenny): add material selection
	leaf->surface_vertex_id = surface.addVertex({ vertex, normal });
}

struct EdgeProcArgs final {
	std::array<const ChunkOctreeNodeBase *, 4> nodes;
	const ChunkOctree &octree;
	ChunkSurface &surface;
};

template<int D>
void edgeProc(EdgeProcArgs args)
{
	constexpr uint32_t CORNERS_TABLE[3][4][2] = {
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
	for (size_t i = 0; i < 8; i++) {
		const uint32_t node_id = EDGE_PROC_RECURSION_TABLE[D][i][0];
		const ChunkOctreeNodeBase *n = nodes[node_id];

		if (!n->is_leaf) {
			const ChunkOctreeCell *cell = n->castToCell();
			const uint32_t child_id = cell->children_ids[EDGE_PROC_RECURSION_TABLE[D][i][1]];

			sub[i] = args.octree.idToPointer(child_id);

			all_leaves = false;
		} else {
			sub[i] = n;
		}
	}

	if (!all_leaves) {
		for (size_t i = 0; i < 2; i++) {
			args.nodes[0] = sub[SUBEDGE_SHARING_TABLE[D][i][0]];
			args.nodes[1] = sub[SUBEDGE_SHARING_TABLE[D][i][1]];
			args.nodes[2] = sub[SUBEDGE_SHARING_TABLE[D][i][2]];
			args.nodes[3] = sub[SUBEDGE_SHARING_TABLE[D][i][3]];
			edgeProc<D>(args);
		}
		return;
	}

	// All four entries in `nodes` are leaves, guaranteed by above `if`
	const ChunkOctreeLeaf *leaves[4];
	for (size_t i = 0; i < 4; i++) {
		leaves[i] = nodes[i]->castToLeaf();
	}

	voxel_t mat1 = 0, mat2 = 0;
	/* Find the minimal node, i.e. the node with the maximal depth. By looking at its
	 materials on endpoints of this edge we may know whether the edge is surface-crossing
	 and if we need to flip the triangles winding order. */
	int32_t max_depth = INT32_MIN;
	for (size_t i = 0; i < 4; i++) {
		auto depth = static_cast<int32_t>(leaves[i]->depth);

		if (depth > max_depth) {
			max_depth = depth;
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

	uint32_t id0 = leaves[0]->surface_vertex_id;
	uint32_t id1 = leaves[1]->surface_vertex_id;
	uint32_t id2 = leaves[2]->surface_vertex_id;
	uint32_t id3 = leaves[3]->surface_vertex_id;

	if (!flip) {
		args.surface.addTriangle(id0, id1, id2);
		args.surface.addTriangle(id0, id2, id3);
	} else {
		args.surface.addTriangle(id0, id2, id1);
		args.surface.addTriangle(id0, id3, id2);
	}
}

struct FaceProcArgs final {
	std::array<const ChunkOctreeNodeBase *, 2> nodes;
	const ChunkOctree &octree;
	ChunkSurface &surface;

	EdgeProcArgs toEdgeProcArgs() const noexcept
	{
		return EdgeProcArgs {
			.octree = octree,
			.surface = surface
		};
	}
};

template<int D>
void faceProc(FaceProcArgs args)
{
	const std::array<const ChunkOctreeNodeBase *, 2> &nodes = args.nodes;

	if (!nodes[0] || !nodes[1]) {
		// No valid lowest quadruples can be generated
		// from this pair if at least one node doesn't exist
		return;
	}

	const ChunkOctreeNodeBase *sub[8];

	bool has_cells = false;
	for (int i = 0; i < 8; i++) {
		const uint32_t node_id = FACE_PROC_RECURSION_TABLE[D][i][0];
		const ChunkOctreeNodeBase *n = nodes[node_id];

		if (!n->is_leaf) {
			const ChunkOctreeCell *cell = n->castToCell();
			const uint32_t child_id = cell->children_ids[FACE_PROC_RECURSION_TABLE[D][i][1]];

			sub[i] = args.octree.idToPointer(child_id);

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
		faceProc<D>(args);
	}

	EdgeProcArgs edge_args = args.toEdgeProcArgs();

	constexpr int D1 = (D + 1) % 3;
	for (size_t i = 0; i < 2; i++) {
		for (size_t j = 0; j < 4; j++) {
			uint32_t idx = SUBEDGE_SHARING_TABLE[D1][i][j];
			edge_args.nodes[j] = sub[idx];
		}

		edgeProc<D1>(edge_args);
	}

	constexpr int D2 = (D + 2) % 3;
	for (size_t i = 0; i < 2; i++) {
		for (size_t j = 0; j < 4; j++) {
			uint32_t idx = SUBEDGE_SHARING_TABLE[D2][i][j];
			edge_args.nodes[j] = sub[idx];
		}

		edgeProc<D2>(edge_args);
	}
}

void cellProc(const ChunkOctreeNodeBase *node, const ChunkOctree &octree, ChunkSurface &surface)
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

	FaceProcArgs face_args {
		.octree = octree,
		.surface = surface
	};
	for (int i = 0; i < 4; i++) {
		face_args.nodes = { sub[SUBFACE_SHARING_TABLE[0][i][0]], sub[SUBFACE_SHARING_TABLE[0][i][1]] };
		faceProc<0>(face_args);

		face_args.nodes = { sub[SUBFACE_SHARING_TABLE[1][i][0]], sub[SUBFACE_SHARING_TABLE[1][i][1]] };
		faceProc<1>(face_args);

		face_args.nodes = { sub[SUBFACE_SHARING_TABLE[2][i][0]], sub[SUBFACE_SHARING_TABLE[2][i][1]] };
		faceProc<2>(face_args);
	}

	EdgeProcArgs edge_args {
		.octree = octree,
		.surface = surface
	};
	for (int i = 0; i < 2; i++) {
		edge_args.nodes = { sub[SUBEDGE_SHARING_TABLE[0][i][0]], sub[SUBEDGE_SHARING_TABLE[0][i][1]],
		                    sub[SUBEDGE_SHARING_TABLE[0][i][2]], sub[SUBEDGE_SHARING_TABLE[0][i][3]] };
		edgeProc<0>(edge_args);

		edge_args.nodes = { sub[SUBEDGE_SHARING_TABLE[1][i][0]], sub[SUBEDGE_SHARING_TABLE[1][i][1]],
		                    sub[SUBEDGE_SHARING_TABLE[1][i][2]], sub[SUBEDGE_SHARING_TABLE[1][i][3]] };
		edgeProc<1>(edge_args);

		edge_args.nodes = { sub[SUBEDGE_SHARING_TABLE[2][i][0]], sub[SUBEDGE_SHARING_TABLE[2][i][1]],
		                    sub[SUBEDGE_SHARING_TABLE[2][i][2]], sub[SUBEDGE_SHARING_TABLE[2][i][3]] };
		edgeProc<2>(edge_args);
	}
}

void makeVertices(ChunkOctreeNodeBase *node, ChunkOctree &octree, ChunkSurface &surface)
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
		assert(false);
		__builtin_unreachable();
	}
}

struct DcBuildArgs {
	ChunkOctree &octree;
	const ChunkPrimaryData &primary_data;
	QefSolver3D &solver;
	const float epsilon;
};

std::pair<uint32_t, ChunkOctreeLeaf *> buildLeaf(glm::uvec3 min_corner, uint32_t size, int8_t depth, DcBuildArgs &args)
{
	using coord_t = HermiteDataStorage::coord_t;

	QefSolver3D &solver = args.solver;

	solver.reset();
	glm::vec3 avg_normal { 0 };

	const auto &grid = args.primary_data.voxel_grid;
	std::array<voxel_t, 8> corners = grid.getCellLinear(min_corner.x, min_corner.y, min_corner.z);

	bool has_edges = false;

	constexpr uint32_t edge_table[3][4][2] = {
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
			const auto iter = storage.find(coord_t(edge_pos.x), coord_t(edge_pos.y), coord_t(edge_pos.z));
			assert(iter != storage.cend());

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
	constexpr uint32_t DC_TO_MC[8] = { 0, 3, 1, 2, 4, 7, 5, 6 };
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
	for (size_t i = 0; i < 8; i++) {
		auto pos = 2u * CELL_CORNER_OFFSET_TABLE[i];
		voxel_t mat = mats[pos.y][pos.x][pos.z];
		if (mat != 0)
			mask |= 1u << DC_TO_MC[i];
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
				submask |= 1u << DC_TO_MC[j];
		}
		if (!IS_MANIFOLD[submask]) {
			// Node already has ambiguous configuration
			return false;
		}
	}

	// Check edge midpoint signs (they must be equal to either edge endpoint)
	for (size_t c1 = 0; c1 < 3; c1++) {
		for (size_t c2 = 0; c2 < 3; c2++) {
			if (mats[1][c1][c2] != mats[0][c1][c2] && mats[1][c1][c2] != mats[2][c1][c2])
				return false;
			if (mats[c1][1][c2] != mats[c1][0][c2] && mats[c1][1][c2] != mats[c1][2][c2])
				return false;
			if (mats[c1][c2][1] != mats[c1][c2][0] && mats[c1][c2][1] != mats[c1][c2][2])
				return false;
		}
	}

	// Check face midpoint signs (they must be equal to either face corner)
	for (size_t c1 = 0; c1 < 3; c1++) {
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
	for (size_t i = 0; i < 8; i++) {
		auto pos = 2u * CELL_CORNER_OFFSET_TABLE[i];
		if (mat == mats[pos.y][pos.x][pos.z]) {
			// All checks passed, this topology is safe to collapse
			return true;
		}
	}

	// Cube midpoint sign check failed
	return false;
}

std::pair<uint32_t, ChunkOctreeNodeBase *> buildNode(glm::uvec3 min_corner, uint32_t size,
                                                     int8_t depth, DcBuildArgs &args)
{
	assert(size > 0);
	if (size == 1) {
		return buildLeaf(min_corner, size, depth, args);
	}

	uint32_t children_ids[8];
	bool has_children = false;
	bool has_child_cell = false;

	const uint32_t child_size = size / 2u;
	for (size_t i = 0; i < 8; i++) {
		glm::uvec3 child_min_corner = min_corner + child_size * CELL_CORNER_OFFSET_TABLE[i];
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
	for (size_t i = 0; i < 8; i++) {
		if (children_ptrs[i]) {
			ChunkOctreeLeaf *leaf = children_ptrs[i]->castToLeaf();
			// `i` is a bitmask of per-axis offsets, i.e. 5 -> 101 means offset in Y and Z axes.
			// `i ^ 7` is an inverse mask, i.e. 5 ^ 7 = 2 -> 010 means offset in X axis.
			// `i`-th children's `i ^ 7`-th corner will always be in the center of parent cell.
			center_mat = leaf->corners[i ^ 7u];
			break;
		}
	}
	for (size_t i = 0; i < 3; i++) {
		for (size_t j = 0; j < 3; j++) {
			mats[i][j].fill(center_mat);
		}
	}

	// Now fill the rest
	for (size_t i = 0; i < 8; i++) {
		if (!children_ptrs[i]) {
			continue;
		}
		ChunkOctreeLeaf *leaf = children_ptrs[i]->castToLeaf();
		for (size_t j = 0; j < 8; j++) {
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
	for (size_t i = 0; i < 8; i++) {
		auto offset = 2u * CELL_CORNER_OFFSET_TABLE[i];
		corners[i] = mats[offset.y][offset.x][offset.z];
	}

	auto &solver = args.solver;

	// Now try to combine childrens' QEF's
	solver.reset();
	glm::vec3 avg_normal { 0 };
	for (size_t i = 0; i < 8; i++) {
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
	for (size_t i = 0; i < 8; i++) {
		args.octree.freeNode(children_ids[i]);
	}

	auto[id, leaf] = args.octree.allocLeaf(depth);
	leaf->surface_vertex = surface_vertex;
	leaf->surface_normal = glm::normalize(avg_normal);
	leaf->corners = corners;
	leaf->qef_state = solver.state();

	return { id, leaf };
}

} // end anonymous namespace

void SurfaceBuilder::buildOctree(Chunk &chunk)
{
	ChunkOctree &octree = chunk.octree();
	octree.clear();

	ChunkPrimaryData &primary = chunk.primaryData();
	if (primary.hermite_data_x.empty() && primary.hermite_data_y.empty() && primary.hermite_data_z.empty()) {
		// This chunk has no edges - no need to even spend any time trying to build octree
		return;
	}

	QefSolver3D qef_solver;
	DcBuildArgs args {
		.octree = octree,
		.primary_data = primary,
		.solver = qef_solver,
		.epsilon = 0.12f
	};

	auto[root_id, root] = buildNode(glm::uvec3(0), Config::CHUNK_SIZE, 0, args);
	octree.setRoot(root_id);
}

void SurfaceBuilder::buildSurface(Chunk &chunk)
{
	ChunkOctree &octree = chunk.octree();
	ChunkSurface &surface = chunk.surface();
	surface.clear();

	auto *root = octree.idToPointer(octree.root());
	makeVertices(root, octree, surface);
	cellProc(root, octree, surface);
}

}
