#include <voxen/common/terrain.hpp>

#include <voxen/common/terrain/cache.hpp>
#include <voxen/common/terrain/octree_tables.hpp>
#include <voxen/common/terrain/seam.hpp>
#include <voxen/common/terrain/surface_builder.hpp>
#include <voxen/util/log.hpp>

#include <algorithm>
#include <queue>

namespace voxen
{

// --- TerrainOctree nodes ---

struct TerrainOctreeNode {
	TerrainOctreeNode(int64_t base_x, int64_t base_y, int64_t base_z, int64_t size, TerrainLoader &loader) :
		m_base_x(base_x), m_base_y(base_y), m_base_z(base_z), m_size(size)
	{
		for (int i = 0; i < 8; i++)
			m_children[i] = nullptr;

		createChunk(loader);
	}

	TerrainOctreeNode(TerrainOctreeNode &&) = delete;
	TerrainOctreeNode &operator = (TerrainOctreeNode &&) = delete;
	TerrainOctreeNode &operator = (const TerrainOctreeNode &) = delete;

	TerrainOctreeNode(const TerrainOctreeNode &other) :
		m_base_x(other.m_base_x), m_base_y(other.m_base_y), m_base_z(other.m_base_z), m_size(other.m_size),
		m_is_collapsed(other.m_is_collapsed), m_seam_set_old(other.m_seam_set_old), m_seam_set_new(other.m_seam_set_new)
	{
		for (int i = 0; i < 8; i++) {
			if (other.m_children[i])
				m_children[i] = new TerrainOctreeNode(*other.m_children[i]);
			else
				m_children[i] = nullptr;
		}
		if (other.m_chunk) {
			m_chunk = new TerrainChunk(*other.m_chunk);
		}
		if (other.m_is_editing) {
			auto[_, secondary] = m_chunk->beginEdit();
			m_is_editing = true;
			m_mutable_secondary = &secondary;
		}
	}

	~TerrainOctreeNode() noexcept
	{
		for (int i = 0; i < 8; i++)
			delete m_children[i];
		delete m_chunk;
	}

	void unload(TerrainOctreeNode* node, TerrainLoader &loader)
	{
		if (node->m_is_collapsed) {
			assert(node->m_chunk);
			loader.unload(*node->m_chunk);
		}
		else {
			assert(!node->m_chunk);
			for (int i = 0; i < 8; i++) {
				assert(node->m_children[i]);
				unload(node->m_children[i], loader);
			}
		}
	}

	void updateChunks(double x, double y, double z, TerrainLoader &loader)
	{
		if (m_size == TerrainChunk::SIZE)
			return;
		double center_x = double(m_size) * 0.5 + double(m_base_x);
		double center_y = double(m_size) * 0.5 + double(m_base_y);
		double center_z = double(m_size) * 0.5 + double(m_base_z);
		double dx = center_x - x;
		double dy = center_y - y;
		double dz = center_z - z;
		double dist = sqrt(dx * dx + dy * dy + dz * dz);
		if (dist < 2 * m_size) {
			//if (is_collapsed)
			//	Log::debug()("Splitting octree node ({}, {}, {}) [{}]", m_base_x, m_base_y, m_base_z, m_size);
			split(loader);
			for (int i = 0; i < 8; i++)
				if (m_children[i])
					m_children[i]->updateChunks(x, y, z, loader);
		}
		else if (dist > 3 * m_size) {
			//if (!is_collapsed)
			//	Log::debug()("Collapsing octree node ({}, {}, {}) [{}]", m_base_x, m_base_y, m_base_z, m_size);
			collapse(loader);
		}
	}

	void split(TerrainLoader &loader)
	{
		if (!m_is_collapsed)
			return;
		loader.unload(*m_chunk);
		delete m_chunk;
		m_chunk = nullptr;
		m_is_editing = false;
		m_mutable_secondary = nullptr;
		int64_t child_size = m_size / 2;
		for (int i = 0; i < 8; i++) {
			if (!m_children[i]) {
				int64_t base_x = m_base_x + child_size * CELL_CORNER_OFFSET_TABLE[i][0];
				int64_t base_y = m_base_y + child_size * CELL_CORNER_OFFSET_TABLE[i][1];
				int64_t base_z = m_base_z + child_size * CELL_CORNER_OFFSET_TABLE[i][2];
				m_children[i] = new TerrainOctreeNode(base_x, base_y, base_z, child_size, loader);
			}
		}
		m_is_collapsed = false;
	}

	void collapse(TerrainLoader &loader)
	{
		if (m_is_collapsed)
			return;
		for (int i = 0; i < 8; i++) {
			unload(m_children[i], loader);
			delete m_children[i];
			m_children[i] = nullptr;
		}
		if (!m_chunk) {
			createChunk(loader);
		}
		m_is_collapsed = true;
	}

	void createChunk(TerrainLoader &loader)
	{
		TerrainChunkHeader header;
		header.scale = m_size / TerrainChunk::SIZE;
		header.base_x = m_base_x;
		header.base_y = m_base_y;
		header.base_z = m_base_z;
		m_chunk = new TerrainChunk(header);

		loader.load(*m_chunk);
		auto[_, secondary] = m_chunk->beginEdit();
		m_is_editing = true;
		m_mutable_secondary = &secondary;
	}

	void finalizeEditing()
	{
		if (m_chunk) {
			/*if (!m_is_editing && m_seam_set_new != m_seam_set_old) {
				auto[primary, secondary] = m_chunk->beginEdit();
				m_is_editing = true;
				m_mutable_secondary = &secondary;
			}*/

			if (m_is_editing) {
				m_seam_set_new.extendOctree(m_chunk->header(), m_mutable_secondary->octree);
				TerrainSurfaceBuilder::buildSurface(*m_mutable_secondary);

				m_mutable_secondary = nullptr;
				m_is_editing = false;
				m_chunk->endEdit();
			}

			std::swap(m_seam_set_new, m_seam_set_old);
			m_seam_set_new.clear();
		}

		for (int i = 0; i < 8; i++) {
			if (m_children[i]) {
				m_children[i]->finalizeEditing();
			}
		}
	}

	int64_t m_base_x, m_base_y, m_base_z, m_size;
	TerrainOctreeNode *m_children[8];
	TerrainChunk *m_chunk = nullptr;
	bool m_is_collapsed = true;

	TerrainChunkSeamSet m_seam_set_old, m_seam_set_new;

	bool m_is_editing = false;
	TerrainChunkSecondaryData *m_mutable_secondary = nullptr;
};

// --- Seam building ---

template<int D>
static void seamEdgeProc(std::array<TerrainOctreeNode *, 4> nodes)
{
	if (!nodes[0] || !nodes[1] || !nodes[2] || !nodes[3]) {
		return;
	}

	std::array<TerrainOctreeNode *, 8> sub;
	bool has_children = false;
	for (int i = 0; i < 8; i++) {
		int node_id = EDGE_PROC_RECURSION_TABLE[D][i][0];
		int child_id = EDGE_PROC_RECURSION_TABLE[D][i][1];
		sub[i] = nodes[node_id]->m_children[child_id];
		if (!sub[i]) {
			sub[i] = nodes[node_id];
		} else {
			has_children = true;
		}
	}

	if (!has_children) {
		nodes[0]->m_seam_set_new.addEdgeRef<D>(nodes[2]->m_chunk);
		return;
	}

	for (int i = 0; i < 2; i++) {
		int i1 = SUBEDGE_SHARING_TABLE[D][i][0];
		int i2 = SUBEDGE_SHARING_TABLE[D][i][1];
		int i3 = SUBEDGE_SHARING_TABLE[D][i][2];
		int i4 = SUBEDGE_SHARING_TABLE[D][i][3];
		seamEdgeProc<D>({ sub[i1], sub[i2], sub[i3], sub[i4] });
	}
}

template<int D>
static void seamFaceProc(std::array<TerrainOctreeNode *, 2> nodes)
{
	if (!nodes[0] || !nodes[1]) {
		return;
	}

	std::array<TerrainOctreeNode *, 8> sub;
	bool has_children = false;
	for (int i = 0; i < 8; i++) {
		int node_id = FACE_PROC_RECURSION_TABLE[D][i][0];
		int child_id = FACE_PROC_RECURSION_TABLE[D][i][1];
		sub[i] = nodes[node_id]->m_children[child_id];
		if (!sub[i]) {
			sub[i] = nodes[node_id];
		} else {
			has_children = true;
		}
	}

	if (!has_children) {
		nodes[0]->m_seam_set_new.addFaceRef<D>(nodes[1]->m_chunk);
		return;
	}

	for (int i = 0; i < 4; i++) {
		int i1 = SUBFACE_SHARING_TABLE[D][i][0];
		int i2 = SUBFACE_SHARING_TABLE[D][i][1];
		seamFaceProc<D>({ sub[i1], sub[i2] });
	}
	constexpr int D1 = (D + 1) % 3;
	for (int i = 0; i < 2; i++) {
		int i1 = SUBEDGE_SHARING_TABLE[D1][i][0];
		int i2 = SUBEDGE_SHARING_TABLE[D1][i][1];
		int i3 = SUBEDGE_SHARING_TABLE[D1][i][2];
		int i4 = SUBEDGE_SHARING_TABLE[D1][i][3];
		seamEdgeProc<D1>({ sub[i1], sub[i2], sub[i3], sub[i4] });
	}
	constexpr int D2 = (D + 2) % 3;
	for (int i = 0; i < 2; i++) {
		int i1 = SUBEDGE_SHARING_TABLE[D2][i][0];
		int i2 = SUBEDGE_SHARING_TABLE[D2][i][1];
		int i3 = SUBEDGE_SHARING_TABLE[D2][i][2];
		int i4 = SUBEDGE_SHARING_TABLE[D2][i][3];
		seamEdgeProc<D2>({ sub[i1], sub[i2], sub[i3], sub[i4] });
	}
}

static void seamCellProc(TerrainOctreeNode *node)
{
	if (!node) {
		return;
	}

	if (node->m_is_collapsed) {
		return;
	}

	std::array<TerrainOctreeNode *, 8> sub;
	for (int i = 0; i < 8; i++) {
		sub[i] = node->m_children[i];
		seamCellProc(sub[i]);
	}
	for (int i = 0; i < 4; i++) {
		seamFaceProc<0>({ sub[SUBFACE_SHARING_TABLE[0][i][0]], sub[SUBFACE_SHARING_TABLE[0][i][1]] });
		seamFaceProc<1>({ sub[SUBFACE_SHARING_TABLE[1][i][0]], sub[SUBFACE_SHARING_TABLE[1][i][1]] });
		seamFaceProc<2>({ sub[SUBFACE_SHARING_TABLE[2][i][0]], sub[SUBFACE_SHARING_TABLE[2][i][1]] });
	}
	for (int i = 0; i < 2; i++) {
		seamEdgeProc<0>({ sub[SUBEDGE_SHARING_TABLE[0][i][0]], sub[SUBEDGE_SHARING_TABLE[0][i][1]],
		                  sub[SUBEDGE_SHARING_TABLE[0][i][2]], sub[SUBEDGE_SHARING_TABLE[0][i][3]] });
		seamEdgeProc<1>({ sub[SUBEDGE_SHARING_TABLE[1][i][0]], sub[SUBEDGE_SHARING_TABLE[1][i][1]],
		                  sub[SUBEDGE_SHARING_TABLE[1][i][2]], sub[SUBEDGE_SHARING_TABLE[1][i][3]] });
		seamEdgeProc<2>({ sub[SUBEDGE_SHARING_TABLE[2][i][0]], sub[SUBEDGE_SHARING_TABLE[2][i][1]],
		                  sub[SUBEDGE_SHARING_TABLE[2][i][2]], sub[SUBEDGE_SHARING_TABLE[2][i][3]] });
	}
}

// --- TerrainOctree ---

inline bool isPowerOfTwo(uint32_t num) noexcept { return (num & (num - 1)) == 0; }

TerrainOctree::TerrainOctree(TerrainLoader &loader, uint32_t num_xz_chunks, uint32_t num_y_chunks) :
	m_xz_chunks(num_xz_chunks), m_y_chunks(num_y_chunks)
{
	assert(num_xz_chunks > 0 && isPowerOfTwo(num_xz_chunks));
	assert(num_y_chunks > 0 && isPowerOfTwo(num_y_chunks));
	assert(num_xz_chunks >= num_y_chunks);

	// TODO (Svenny): currently no multiple highest-level nodes are supported
	assert(num_xz_chunks == num_y_chunks);

	int64_t node_size = int64_t(TerrainChunk::SIZE) * int64_t(num_xz_chunks);
	int64_t base_coord = -node_size / 2;
	m_tree = new TerrainOctreeNode(base_coord, base_coord, base_coord, node_size, loader);
	Log::info("Created terrain octree with {} XZ and {} Y chunks", m_xz_chunks, m_y_chunks);
}

TerrainOctree::TerrainOctree(TerrainOctree &&other) noexcept :
	m_xz_chunks(other.m_xz_chunks), m_y_chunks(other.m_y_chunks)
{
	m_tree = std::exchange(other.m_tree, nullptr);
}

TerrainOctree::TerrainOctree(const TerrainOctree &other) :
	m_xz_chunks(other.m_xz_chunks), m_y_chunks(other.m_y_chunks)
{
	if (other.m_tree)
		m_tree = new TerrainOctreeNode(*other.m_tree);
	else
		m_tree = nullptr;
}

TerrainOctree::~TerrainOctree() noexcept
{
	delete m_tree;
}

void TerrainOctree::updateChunks(double x, double y, double z, TerrainLoader &loader)
{
	assert(m_tree);
	// First load/unload the chunks (primary data)
	m_tree->updateChunks(x, y, z, loader);
	// Then recalculate the seamsets
	seamCellProc(m_tree);
	// Then update seams where changed and commit the changes
	m_tree->finalizeEditing();
}

void TerrainOctree::walkActiveChunks(std::function<void(const TerrainChunk &)> visitor) const
{
	if (!m_tree)
		return;

	std::vector<const TerrainOctreeNode *> stack;
	stack.emplace_back(m_tree);

	while (!stack.empty()) {
		const TerrainOctreeNode *node = stack.back();
		stack.pop_back();

		if (node->m_is_collapsed) {
			TerrainChunk *chunk = node->m_chunk;
			if (chunk)
				visitor(*chunk);
			continue;
		}

		for (size_t i = 0; i < 8; i++) {
			if (node->m_children[i])
				stack.emplace_back(node->m_children[i]);
		}
	}
}

}
