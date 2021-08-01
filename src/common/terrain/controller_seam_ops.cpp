#include <voxen/common/terrain/controller.hpp>

#include <voxen/common/terrain/chunk_octree.hpp>
#include <voxen/common/terrain/control_block.hpp>
#include <voxen/common/terrain/octree_tables.hpp>

namespace voxen::terrain
{

using RecursionTable = int[8][2];

template<size_t N>
static bool canProceedPhase1(const std::array<ChunkControlBlock *, N> &nodes) noexcept
{
	for (size_t i = 0; i < N; i++) {
		if (!nodes[i]) {
			return false;
		}
	}

	for (size_t i = 1; i < N; i++) {
		if (nodes[i]->isChunkChanged()) {
			return true;
		}
	}

	return false;
}

template<size_t N>
static bool canProceedPhase2(const std::array<ChunkControlBlock *, N> &nodes) noexcept
{
	for (size_t i = 0; i < N; i++) {
		if (!nodes[i]) {
			return false;
		}
	}
	// We will only rebuild seams of `nodes[0]`, so don't care about other nodes' flags
	return nodes[0]->isInducedSeamDirty();
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
	return node.chunk()->hasSurface();
}

template<int D>
void Controller::seamEdgeProcPhase1(std::array<ChunkControlBlock *, 4> nodes)
{
	if (!canProceedPhase1(nodes)) {
		return;
	}

	std::array<ChunkControlBlock *, 8> sub;

	bool has_children = getSubNodes(nodes, sub, EDGE_PROC_RECURSION_TABLE[D]);
	if (!has_children) {
		if (needRebuildSeam(*nodes[0])) {
			nodes[0]->setInducedSeamDirty(true);
		}
		return;
	}

	for (int i = 0; i < 2; i++) {
		int i1 = SUBEDGE_SHARING_TABLE[D][i][0];
		int i2 = SUBEDGE_SHARING_TABLE[D][i][1];
		int i3 = SUBEDGE_SHARING_TABLE[D][i][2];
		int i4 = SUBEDGE_SHARING_TABLE[D][i][3];
		seamEdgeProcPhase1<D>({ sub[i1], sub[i2], sub[i3], sub[i4] });
	}

	for (int i = 0; i < 8; i++) {
		if (sub[i] && sub[i]->isInducedSeamDirty()) {
			nodes[EDGE_PROC_RECURSION_TABLE[D][i][0]]->setInducedSeamDirty(true);
		}
	}
}

template<int D>
void Controller::seamFaceProcPhase1(std::array<ChunkControlBlock *, 2> nodes)
{
	if (!canProceedPhase1(nodes)) {
		return;
	}

	std::array<ChunkControlBlock *, 8> sub;

	bool has_children = getSubNodes(nodes, sub, FACE_PROC_RECURSION_TABLE[D]);
	if (!has_children) {
		if (needRebuildSeam(*nodes[0])) {
			nodes[0]->setInducedSeamDirty(true);
		}
		return;
	}

	for (int i = 0; i < 4; i++) {
		int i1 = SUBFACE_SHARING_TABLE[D][i][0];
		int i2 = SUBFACE_SHARING_TABLE[D][i][1];
		seamFaceProcPhase1<D>({ sub[i1], sub[i2] });
	}

	constexpr int D1 = (D + 1) % 3;
	for (int i = 0; i < 2; i++) {
		int i1 = SUBEDGE_SHARING_TABLE[D1][i][0];
		int i2 = SUBEDGE_SHARING_TABLE[D1][i][1];
		int i3 = SUBEDGE_SHARING_TABLE[D1][i][2];
		int i4 = SUBEDGE_SHARING_TABLE[D1][i][3];
		seamEdgeProcPhase1<D1>({ sub[i1], sub[i2], sub[i3], sub[i4] });
	}
	constexpr int D2 = (D + 2) % 3;
	for (int i = 0; i < 2; i++) {
		int i1 = SUBEDGE_SHARING_TABLE[D2][i][0];
		int i2 = SUBEDGE_SHARING_TABLE[D2][i][1];
		int i3 = SUBEDGE_SHARING_TABLE[D2][i][2];
		int i4 = SUBEDGE_SHARING_TABLE[D2][i][3];
		seamEdgeProcPhase1<D2>({ sub[i1], sub[i2], sub[i3], sub[i4] });
	}

	for (int i = 0; i < 8; i++) {
		if (sub[i] && sub[i]->isInducedSeamDirty()) {
			nodes[FACE_PROC_RECURSION_TABLE[D][i][0]]->setInducedSeamDirty(true);
		}
	}
}

void Controller::seamCellProcPhase1(ChunkControlBlock *node)
{
	if (!node || !node->isChunkChanged() || node->state() == ChunkControlBlock::State::Active) {
		return;
	}

	std::array<ChunkControlBlock *, 8> sub;
	for (int i = 0; i < 8; i++) {
		sub[i] = node->child(i);
	}

	// Recursively apply `seamFaceProc` to pairs of face-sharing children
	for (int i = 0; i < 4; i++) {
		int i1, i2;

		i1 = SUBFACE_SHARING_TABLE[0][i][0];
		i2 = SUBFACE_SHARING_TABLE[0][i][1];
		seamFaceProcPhase1<0>({ sub[i1], sub[i2] });

		i1 = SUBFACE_SHARING_TABLE[1][i][0];
		i2 = SUBFACE_SHARING_TABLE[1][i][1];
		seamFaceProcPhase1<1>({ sub[i1], sub[i2] });

		i1 = SUBFACE_SHARING_TABLE[2][i][0];
		i2 = SUBFACE_SHARING_TABLE[2][i][1];
		seamFaceProcPhase1<2>({ sub[i1], sub[i2] });
	}

	// Recursively apply `seamEdgeProc` to quads of edge-sharing children
	for (int i = 0; i < 2; i++) {
		int i1, i2, i3, i4;

		i1 = SUBEDGE_SHARING_TABLE[0][i][0];
		i2 = SUBEDGE_SHARING_TABLE[0][i][1];
		i3 = SUBEDGE_SHARING_TABLE[0][i][2];
		i4 = SUBEDGE_SHARING_TABLE[0][i][3];
		seamEdgeProcPhase1<0>({ sub[i1], sub[i2], sub[i3], sub[i4] });

		i1 = SUBEDGE_SHARING_TABLE[1][i][0];
		i2 = SUBEDGE_SHARING_TABLE[1][i][1];
		i3 = SUBEDGE_SHARING_TABLE[1][i][2];
		i4 = SUBEDGE_SHARING_TABLE[1][i][3];
		seamEdgeProcPhase1<1>({ sub[i1], sub[i2], sub[i3], sub[i4] });

		i1 = SUBEDGE_SHARING_TABLE[2][i][0];
		i2 = SUBEDGE_SHARING_TABLE[2][i][1];
		i3 = SUBEDGE_SHARING_TABLE[2][i][2];
		i4 = SUBEDGE_SHARING_TABLE[2][i][3];
		seamEdgeProcPhase1<2>({ sub[i1], sub[i2], sub[i3], sub[i4] });
	}

	// Recursively apply `seamCellProc` to children
	for (int i = 0; i < 8; i++) {
		seamCellProcPhase1(sub[i]);

		if (sub[i] && sub[i]->isInducedSeamDirty()) {
			// Propagate "induced seam dirty" flag if some child had it set
			node->setInducedSeamDirty(true);
		}
	}
}

template<int D>
void Controller::seamEdgeProcPhase2(std::array<ChunkControlBlock *, 4> nodes)
{
	if (!canProceedPhase2(nodes)) {
		return;
	}

	std::array<ChunkControlBlock *, 8> sub;
	bool has_children = getSubNodes(nodes, sub, EDGE_PROC_RECURSION_TABLE[D]);
	if (!has_children) {
		// No need to waste time updating seams for non-active chunks
		if (nodes[0]->state() == ChunkControlBlock::State::Active) {
			nodes[0]->copyChunk();
			nodes[0]->surfaceBuilder().buildEdgeSeam<D>(*nodes[0]->chunk(), *nodes[1]->chunk(),
			                                            *nodes[2]->chunk(), *nodes[3]->chunk());
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
	if (!canProceedPhase2(nodes)) {
		return;
	}

	std::array<ChunkControlBlock *, 8> sub;
	bool has_children = getSubNodes(nodes, sub, FACE_PROC_RECURSION_TABLE[D]);
	if (!has_children) {
		// No need to waste time updating seams for non-active chunks
		if (nodes[0]->state() == ChunkControlBlock::State::Active) {
			nodes[0]->copyChunk();
			nodes[0]->surfaceBuilder().buildFaceSeam<D>(*nodes[0]->chunk(), *nodes[1]->chunk());
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

static void resetTemporaryFlags(ChunkControlBlock *node)
{
	node->clearTemporaryFlags();
	for (int i = 0; i < 8; i++) {
		ChunkControlBlock *child = node->child(i);
		if (child && child->isInducedSeamDirty()) {
			resetTemporaryFlags(child);
		}
	}
}

void Controller::seamCellProcPhase2(ChunkControlBlock *node)
{
	if (!node || !node->isInducedSeamDirty()) {
		return;
	}

	if (node->state() == ChunkControlBlock::State::Active) {
		// We've reached active node. No need to go deeper,
		// there will be no active-active contact points.
		resetTemporaryFlags(node);
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

	// Recursively apply `seamCellProc` to children. Note it's applied after
	// face and edge functions to avoid resetting "seam dirty" flag too early.
	for (int i = 0; i < 8; i++) {
		seamCellProcPhase2(sub[i]);
	}

	// Reset flags as the last step, we're guaranteed this node will not be visited again
	resetTemporaryFlags(node);
}

void Controller::updateCrossSuperchunkSeams()
{
	for (auto &[base, info] : m_superchunks) {
		auto get_cb = [&](const glm::ivec3 &pos) -> ChunkControlBlock * {
			auto iter = m_superchunks.find(pos);
			if (iter == m_superchunks.end()) {
				return nullptr;
			}

			return iter->second.ptr.get();
		};

		ChunkControlBlock *me = info.ptr.get();
		auto *cb_x = get_cb(base + glm::ivec3(1, 0, 0));
		auto *cb_y = get_cb(base + glm::ivec3(0, 1, 0));
		auto *cb_z = get_cb(base + glm::ivec3(0, 0, 1));
		auto *cb_xy = get_cb(base + glm::ivec3(1, 1, 0));
		auto *cb_xz = get_cb(base + glm::ivec3(1, 0, 1));
		auto *cb_yz = get_cb(base + glm::ivec3(0, 1, 1));

		seamFaceProcPhase1<0>({ me, cb_x });
		seamFaceProcPhase1<1>({ me, cb_y });
		seamFaceProcPhase1<2>({ me, cb_z });
		seamEdgeProcPhase1<0>({ me, cb_y, cb_yz, cb_z });
		seamEdgeProcPhase1<1>({ me, cb_z, cb_xz, cb_x });
		seamEdgeProcPhase1<2>({ me, cb_x, cb_xy, cb_y });

		seamFaceProcPhase2<0>({ me, cb_x });
		seamFaceProcPhase2<1>({ me, cb_y });
		seamFaceProcPhase2<2>({ me, cb_z });
		seamEdgeProcPhase2<0>({ me, cb_y, cb_yz, cb_z });
		seamEdgeProcPhase2<1>({ me, cb_z, cb_xz, cb_x });
		seamEdgeProcPhase2<2>({ me, cb_x, cb_xy, cb_y });
	}
}

}
