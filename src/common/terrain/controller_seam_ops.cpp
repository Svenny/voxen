#include <voxen/common/terrain/controller.hpp>

#include <voxen/common/terrain/chunk_octree.hpp>
#include <voxen/common/terrain/control_block.hpp>
#include <voxen/common/terrain/octree_tables.hpp>

namespace voxen::terrain
{

using RecursionTable = int[8][2];

template<size_t N>
static bool canProceed(const std::array<ChunkControlBlock *, N> &nodes) noexcept
{
	for (size_t i = 0; i < N; i++) {
		if (!nodes[i]) {
			return false;
		}
		if (nodes[i]->isSeamDirty()) {
			return true;
		}
	}

	return false;
}

template<size_t N>
static bool getSubNodes(const std::array<ChunkControlBlock *, N> &nodes,
                        std::array<ChunkControlBlock *, 8> &sub,
                        const RecursionTable &table) noexcept
{
	bool has_children = false;

	for (int i = 0; i < 8; i++) {
		auto *node = nodes[table[i][0]];
		auto *child = node->child(table[i][1]);

		if (node->state() == ChunkControlBlock::State::Active || !child) {
			sub[i] = node;
		} else {
			sub[i] = child;
			has_children = true;
		}
	}

	return has_children;
}

static bool needRebuildSeam(const ChunkControlBlock &node) noexcept
{
	if (node.state() != ChunkControlBlock::State::Active) {
		return false;
	}
	// Ignore surfaceless chunks - they can't contribute to the seam
	return node.chunk()->octree().root() != ChunkOctree::INVALID_NODE_ID;
}

template<int D>
std::array<Controller::OuterUpdateResult, 4> Controller::seamEdgeProcPhase1(std::array<ChunkControlBlock *, 4> nodes)
{
	if (!canProceed(nodes)) {
		return {};
	}

	std::array<OuterUpdateResult, 4> new_node_ptrs;
	std::array<ChunkControlBlock *, 8> sub;

	// Replace `nodes[i]` with COW-copy if it hasn't been COW-copied in this tick yet
	auto conditional_cow = [&](int i) {
		if (new_node_ptrs[i]) {
			return;
		}

		if (auto iter = m_new_cbs.find(nodes[i]); iter != m_new_cbs.end()) {
			(*iter)->setSeamDirty(true);
			new_node_ptrs[i] = ControlBlockPtr();
			return;
		}

		new_node_ptrs[i] = copyOnWrite(*nodes[i], true);
		nodes[i] = (*new_node_ptrs[i]).get();
	};

	// Check if `sub[i]` has changed and reflect these changes in `nodes[X]`
	auto check_subproc = [&](OuterUpdateResult new_ptr, int i) {
		if (!new_ptr.has_value()) {
			return;
		}

		const int node_id = EDGE_PROC_RECURSION_TABLE[D][i][0];
		const int child_id = EDGE_PROC_RECURSION_TABLE[D][i][1];

		if (sub[i] == nodes[node_id]) {
			// This node had no children, it has to be treated specially
			// No COW needed - it was already done by subproc if needed
			if (*new_ptr) {
				new_node_ptrs[node_id] = std::move(new_ptr);
				nodes[node_id] = (*new_node_ptrs[node_id]).get();
				// Update subnodes' pointers
				for (int i = 0; i < 8; i++) {
					if (EDGE_PROC_RECURSION_TABLE[D][i][0] == node_id) {
						sub[i] = nodes[node_id];
					}
				}
			}
		} else {
			conditional_cow(node_id);

			if (*new_ptr) {
				sub[i] = (*new_ptr).get();
				nodes[node_id]->setChild(child_id, *std::move(new_ptr));
			}
		}
	};

	bool has_children = getSubNodes(nodes, sub, EDGE_PROC_RECURSION_TABLE[D]);
	if (!has_children) {
		if (needRebuildSeam(*nodes[0])) {
			conditional_cow(0);
		}
		return new_node_ptrs;
	}

	auto check_edge_proc = [&](std::array<OuterUpdateResult, 4> new_ptrs, int i1, int i2, int i3, int i4) {
		check_subproc(std::move(new_ptrs[0]), i1);
		check_subproc(std::move(new_ptrs[1]), i2);
		check_subproc(std::move(new_ptrs[2]), i3);
		check_subproc(std::move(new_ptrs[3]), i4);
	};

	for (int i = 0; i < 2; i++) {
		int i1 = SUBEDGE_SHARING_TABLE[D][i][0];
		int i2 = SUBEDGE_SHARING_TABLE[D][i][1];
		int i3 = SUBEDGE_SHARING_TABLE[D][i][2];
		int i4 = SUBEDGE_SHARING_TABLE[D][i][3];
		check_edge_proc(seamEdgeProcPhase1<D>({ sub[i1], sub[i2], sub[i3], sub[i4] }), i1, i2, i3, i4);
	}

	return new_node_ptrs;
}

template<int D>
std::array<Controller::OuterUpdateResult, 2> Controller::seamFaceProcPhase1(std::array<ChunkControlBlock *, 2> nodes)
{
	if (!canProceed(nodes)) {
		return {};
	}

	std::array<OuterUpdateResult, 2> new_node_ptrs;
	std::array<ChunkControlBlock *, 8> sub;

	// Replace `nodes[i]` with COW-copy if it hasn't been COW-copied in this tick yet
	auto conditional_cow = [&](int i) {
		if (new_node_ptrs[i]) {
			return;
		}

		if (auto iter = m_new_cbs.find(nodes[i]); iter != m_new_cbs.end()) {
			(*iter)->setSeamDirty(true);
			new_node_ptrs[i] = ControlBlockPtr();
			return;
		}

		new_node_ptrs[i] = copyOnWrite(*nodes[i], true);
		nodes[i] = (*new_node_ptrs[i]).get();
	};

	// Check if `sub[i]` has changed and reflect these changes in `nodes[X]`
	auto check_subproc = [&](OuterUpdateResult new_ptr, int i) {
		if (!new_ptr.has_value()) {
			return;
		}

		const int node_id = FACE_PROC_RECURSION_TABLE[D][i][0];
		const int child_id = FACE_PROC_RECURSION_TABLE[D][i][1];

		if (sub[i] == nodes[node_id]) {
			// This node had no children, it has to be treated specially
			// No COW needed - it was already done by subproc if needed
			if (*new_ptr) {
				new_node_ptrs[node_id] = std::move(new_ptr);
				nodes[node_id] = (*new_node_ptrs[node_id]).get();
				// Update subnodes' pointers
				for (int i = 0; i < 8; i++) {
					if (FACE_PROC_RECURSION_TABLE[D][i][0] == node_id) {
						sub[i] = nodes[node_id];
					}
				}
			}
		} else {
			conditional_cow(node_id);

			if (*new_ptr) {
				sub[i] = (*new_ptr).get();
				nodes[node_id]->setChild(child_id, *std::move(new_ptr));
			}
		}
	};

	bool has_children = getSubNodes(nodes, sub, FACE_PROC_RECURSION_TABLE[D]);
	if (!has_children) {
		if (needRebuildSeam(*nodes[0])) {
			conditional_cow(0);
		}
		return new_node_ptrs;
	}

	auto check_face_proc = [&](std::array<OuterUpdateResult, 2> new_ptrs, int i1, int i2) {
		check_subproc(std::move(new_ptrs[0]), i1);
		check_subproc(std::move(new_ptrs[1]), i2);
	};

	for (int i = 0; i < 4; i++) {
		int i1 = SUBFACE_SHARING_TABLE[D][i][0];
		int i2 = SUBFACE_SHARING_TABLE[D][i][1];
		check_face_proc(seamFaceProcPhase1<D>({ sub[i1], sub[i2] }), i1, i2);
	}

	auto check_edge_proc = [&](std::array<OuterUpdateResult, 4> new_ptrs, int i1, int i2, int i3, int i4) {
		check_subproc(std::move(new_ptrs[0]), i1);
		check_subproc(std::move(new_ptrs[1]), i2);
		check_subproc(std::move(new_ptrs[2]), i3);
		check_subproc(std::move(new_ptrs[3]), i4);
	};

	constexpr int D1 = (D + 1) % 3;
	for (int i = 0; i < 2; i++) {
		int i1 = SUBEDGE_SHARING_TABLE[D1][i][0];
		int i2 = SUBEDGE_SHARING_TABLE[D1][i][1];
		int i3 = SUBEDGE_SHARING_TABLE[D1][i][2];
		int i4 = SUBEDGE_SHARING_TABLE[D1][i][3];
		check_edge_proc(seamEdgeProcPhase1<D1>({ sub[i1], sub[i2], sub[i3], sub[i4] }), i1, i2, i3, i4);
	}
	constexpr int D2 = (D + 2) % 3;
	for (int i = 0; i < 2; i++) {
		int i1 = SUBEDGE_SHARING_TABLE[D2][i][0];
		int i2 = SUBEDGE_SHARING_TABLE[D2][i][1];
		int i3 = SUBEDGE_SHARING_TABLE[D2][i][2];
		int i4 = SUBEDGE_SHARING_TABLE[D2][i][3];
		check_edge_proc(seamEdgeProcPhase1<D2>({ sub[i1], sub[i2], sub[i3], sub[i4] }), i1, i2, i3, i4);
	}

	return new_node_ptrs;
}

Controller::OuterUpdateResult Controller::seamCellProcPhase1(ChunkControlBlock *node)
{
	if (!node || !node->isSeamDirty() || node->state() == ChunkControlBlock::State::Active) {
		return {};
	}

	OuterUpdateResult new_node_ptr;
	std::array<ChunkControlBlock *, 8> sub;

	// Replace `node` with COW-copy if it hasn't been COW-copied in this tick yet
	auto conditional_cow = [&]() {
		if (new_node_ptr.has_value()) {
			return;
		}

		if (auto iter = m_new_cbs.find(node); iter != m_new_cbs.end()) {
			(*iter)->setSeamDirty(true);
			new_node_ptr = ControlBlockPtr();
			return;
		}

		new_node_ptr = copyOnWrite(*node, true);
		node = (*new_node_ptr).get();
	};

	// Check if some child node has changed and reflect these changes in `node`
	auto check_subproc = [&](OuterUpdateResult new_ptr, int i) {
		if (!new_ptr.has_value()) {
			return;
		}

		conditional_cow();

		if (*new_ptr) {
			sub[i] = (*new_ptr).get();
			node->setChild(i, *std::move(new_ptr));
		}
	};

	for (int i = 0; i < 8; i++) {
		sub[i] = node->child(i);
	}

	auto check_face_proc = [&](std::array<OuterUpdateResult, 2> new_ptrs, int i1, int i2) {
		check_subproc(std::move(new_ptrs[0]), i1);
		check_subproc(std::move(new_ptrs[1]), i2);
	};

	for (int i = 0; i < 4; i++) {
		int i1, i2;

		i1 = SUBFACE_SHARING_TABLE[0][i][0];
		i2 = SUBFACE_SHARING_TABLE[0][i][1];
		check_face_proc(seamFaceProcPhase1<0>({ sub[i1], sub[i2] }), i1, i2);

		i1 = SUBFACE_SHARING_TABLE[1][i][0];
		i2 = SUBFACE_SHARING_TABLE[1][i][1];
		check_face_proc(seamFaceProcPhase1<1>({ sub[i1], sub[i2] }), i1, i2);

		i1 = SUBFACE_SHARING_TABLE[2][i][0];
		i2 = SUBFACE_SHARING_TABLE[2][i][1];
		check_face_proc(seamFaceProcPhase1<2>({ sub[i1], sub[i2] }), i1, i2);
	}

	auto check_edge_proc = [&](std::array<OuterUpdateResult, 4> new_ptrs, int i1, int i2, int i3, int i4) {
		check_subproc(std::move(new_ptrs[0]), i1);
		check_subproc(std::move(new_ptrs[1]), i2);
		check_subproc(std::move(new_ptrs[2]), i3);
		check_subproc(std::move(new_ptrs[3]), i4);
	};

	for (int i = 0; i < 2; i++) {
		int i1, i2, i3, i4;

		i1 = SUBEDGE_SHARING_TABLE[0][i][0];
		i2 = SUBEDGE_SHARING_TABLE[0][i][1];
		i3 = SUBEDGE_SHARING_TABLE[0][i][2];
		i4 = SUBEDGE_SHARING_TABLE[0][i][3];
		check_edge_proc(seamEdgeProcPhase1<0>({ sub[i1], sub[i2], sub[i3], sub[i4] }), i1, i2, i3, i4);

		i1 = SUBEDGE_SHARING_TABLE[1][i][0];
		i2 = SUBEDGE_SHARING_TABLE[1][i][1];
		i3 = SUBEDGE_SHARING_TABLE[1][i][2];
		i4 = SUBEDGE_SHARING_TABLE[1][i][3];
		check_edge_proc(seamEdgeProcPhase1<1>({ sub[i1], sub[i2], sub[i3], sub[i4] }), i1, i2, i3, i4);

		i1 = SUBEDGE_SHARING_TABLE[2][i][0];
		i2 = SUBEDGE_SHARING_TABLE[2][i][1];
		i3 = SUBEDGE_SHARING_TABLE[2][i][2];
		i4 = SUBEDGE_SHARING_TABLE[2][i][3];
		check_edge_proc(seamEdgeProcPhase1<2>({ sub[i1], sub[i2], sub[i3], sub[i4] }), i1, i2, i3, i4);
	}

	// Recursively apply `seamCellProc` to children
	for (int i = 0; i < 8; i++) {
		check_subproc(seamCellProcPhase1(sub[i]), i);
	}

	return new_node_ptr;
}

template<int D>
void Controller::seamEdgeProcPhase2(std::array<ChunkControlBlock *, 4> nodes)
{
	if (!canProceed(nodes)) {
		return;
	}

	std::array<ChunkControlBlock *, 8> sub;
	bool has_children = getSubNodes(nodes, sub, EDGE_PROC_RECURSION_TABLE[D]);
	if (!has_children) {
		// No need to waste time updating seams for non-active chunks
		if (nodes[0]->state() == ChunkControlBlock::State::Active) {
			nodes[0]->surfaceBuilder().buildEdgeSeam<D>(*nodes[1]->chunk(), *nodes[2]->chunk(), *nodes[3]->chunk());
		}
		return;
	}

	for (int i = 0; i < 2; i++) {
		int i1 = SUBEDGE_SHARING_TABLE[D][i][0];
		int i2 = SUBEDGE_SHARING_TABLE[D][i][1];
		int i3 = SUBEDGE_SHARING_TABLE[D][i][2];
		int i4 = SUBEDGE_SHARING_TABLE[D][i][3];
		seamEdgeProcPhase2<D>({ sub[i1], sub[i2], sub[i3], sub[i4] });
	}
}

template<int D>
void Controller::seamFaceProcPhase2(std::array<ChunkControlBlock *, 2> nodes)
{
	if (!canProceed(nodes)) {
		return;
	}

	std::array<ChunkControlBlock *, 8> sub;
	bool has_children = getSubNodes(nodes, sub, FACE_PROC_RECURSION_TABLE[D]);
	if (!has_children) {
		// No need to waste time updating seams for non-active chunks
		if (nodes[0]->state() == ChunkControlBlock::State::Active) {
			nodes[0]->surfaceBuilder().buildFaceSeam<D>(*nodes[1]->chunk());
		}
		return;
	}

	for (int i = 0; i < 4; i++) {
		int i1 = SUBFACE_SHARING_TABLE[D][i][0];
		int i2 = SUBFACE_SHARING_TABLE[D][i][1];
		seamFaceProcPhase2<D>({ sub[i1], sub[i2] });
	}

	constexpr int D1 = (D + 1) % 3;
	for (int i = 0; i < 2; i++) {
		int i1 = SUBEDGE_SHARING_TABLE[D1][i][0];
		int i2 = SUBEDGE_SHARING_TABLE[D1][i][1];
		int i3 = SUBEDGE_SHARING_TABLE[D1][i][2];
		int i4 = SUBEDGE_SHARING_TABLE[D1][i][3];
		seamEdgeProcPhase2<D1>({ sub[i1], sub[i2], sub[i3], sub[i4] });
	}
	constexpr int D2 = (D + 2) % 3;
	for (int i = 0; i < 2; i++) {
		int i1 = SUBEDGE_SHARING_TABLE[D2][i][0];
		int i2 = SUBEDGE_SHARING_TABLE[D2][i][1];
		int i3 = SUBEDGE_SHARING_TABLE[D2][i][2];
		int i4 = SUBEDGE_SHARING_TABLE[D2][i][3];
		seamEdgeProcPhase2<D2>({ sub[i1], sub[i2], sub[i3], sub[i4] });
	}
}

void Controller::seamCellProcPhase2(ChunkControlBlock *node)
{
	if (!node || !node->isSeamDirty() || node->state() == ChunkControlBlock::State::Active) {
		if (node) {
			node->setSeamDirty(false);
		}
		return;
	}

	std::array<ChunkControlBlock *, 8> sub;
	for (int i = 0; i < 8; i++) {
		sub[i] = node->child(i);
	}

	for (int i = 0; i < 4; i++) {
		int i1, i2;

		i1 = SUBFACE_SHARING_TABLE[0][i][0];
		i2 = SUBFACE_SHARING_TABLE[0][i][1];
		seamFaceProcPhase2<0>({ sub[i1], sub[i2] });

		i1 = SUBFACE_SHARING_TABLE[1][i][0];
		i2 = SUBFACE_SHARING_TABLE[1][i][1];
		seamFaceProcPhase2<1>({ sub[i1], sub[i2] });

		i1 = SUBFACE_SHARING_TABLE[2][i][0];
		i2 = SUBFACE_SHARING_TABLE[2][i][1];
		seamFaceProcPhase2<2>({ sub[i1], sub[i2] });
	}

	for (int i = 0; i < 2; i++) {
		int i1, i2, i3, i4;

		i1 = SUBEDGE_SHARING_TABLE[0][i][0];
		i2 = SUBEDGE_SHARING_TABLE[0][i][1];
		i3 = SUBEDGE_SHARING_TABLE[0][i][2];
		i4 = SUBEDGE_SHARING_TABLE[0][i][3];
		seamEdgeProcPhase2<0>({ sub[i1], sub[i2], sub[i3], sub[i4] });

		i1 = SUBEDGE_SHARING_TABLE[1][i][0];
		i2 = SUBEDGE_SHARING_TABLE[1][i][1];
		i3 = SUBEDGE_SHARING_TABLE[1][i][2];
		i4 = SUBEDGE_SHARING_TABLE[1][i][3];
		seamEdgeProcPhase2<1>({ sub[i1], sub[i2], sub[i3], sub[i4] });

		i1 = SUBEDGE_SHARING_TABLE[2][i][0];
		i2 = SUBEDGE_SHARING_TABLE[2][i][1];
		i3 = SUBEDGE_SHARING_TABLE[2][i][2];
		i4 = SUBEDGE_SHARING_TABLE[2][i][3];
		seamEdgeProcPhase2<2>({ sub[i1], sub[i2], sub[i3], sub[i4] });
	}

	// Recursively apply `seamCellProc` to children
	for (int i = 0; i < 8; i++) {
		seamCellProcPhase2(sub[i]);
	}

	node->setSeamDirty(false);
}

}
