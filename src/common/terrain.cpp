#include <voxen/common/terrain.hpp>

#include <voxen/common/terrain/cache.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include <algorithm>
#include <queue>

namespace voxen
{

// --- TerrainOctree nodes ---

constexpr int32_t k_child_offset[8][3] = {
   { 0, 0, 0 },
   { 0, 0, 1 },
   { 0, 1, 0 },
   { 0, 1, 1 },
   { 1, 0, 0 },
   { 1, 0, 1 },
   { 1, 1, 0 },
   { 1, 1, 1 }
};

struct TerrainOctreeNode {
	TerrainOctreeNode(int64_t base_x, int64_t base_y, int64_t base_z, int64_t size, TerrainLoader &loader)
	   : m_base_x(base_x), m_base_y(base_y), m_base_z(base_z), m_size(size) {
		for (int i = 0; i < 8; i++)
			m_children[i] = nullptr;

		TerrainChunkCreateInfo info;
		info.scale = m_size / TerrainChunk::SIZE;
		info.base_x = m_base_x;
		info.base_y = m_base_y;
		info.base_z = m_base_z;
		m_chunk = new TerrainChunk(info);
		loader.load(*m_chunk);
	}

	TerrainOctreeNode(TerrainOctreeNode &&) = delete;

	TerrainOctreeNode(const TerrainOctreeNode &other)
	   : m_base_x(other.m_base_x), m_base_y(other.m_base_y), m_base_z(other.m_base_z), m_size(other.m_size), is_collapsed(other.is_collapsed) {
		for (int i = 0; i < 8; i++) {
			if (other.m_children[i])
				m_children[i] = new TerrainOctreeNode(*other.m_children[i]);
			else
				m_children[i] = nullptr;
		}
		if (other.m_chunk)
			m_chunk = new TerrainChunk(*other.m_chunk);
	}

	~TerrainOctreeNode() {
		for (int i = 0; i < 8; i++)
			delete m_children[i];
		delete m_chunk;
	}

	void unload(TerrainOctreeNode* node, TerrainLoader &loader) {
		if (node->is_collapsed) {
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

	void updateChunks(double x, double y, double z, TerrainLoader &loader) {
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

	void split(TerrainLoader &loader) {
		if (!is_collapsed)
			return;
		loader.unload(*m_chunk);
		delete m_chunk;
		m_chunk = nullptr;
		int64_t child_size = m_size / 2;
		for (int i = 0; i < 8; i++) {
			if (!m_children[i]) {
				int64_t base_x = m_base_x + child_size * k_child_offset[i][0];
				int64_t base_y = m_base_y + child_size * k_child_offset[i][1];
				int64_t base_z = m_base_z + child_size * k_child_offset[i][2];
				m_children[i] = new TerrainOctreeNode(base_x, base_y, base_z, child_size, loader);
			}
		}
		is_collapsed = false;
	}

	void collapse(TerrainLoader &loader) {
		if (is_collapsed)
			return;
		for (int i = 0; i < 8; i++) {
			unload(m_children[i], loader);
			delete m_children[i];
			m_children[i] = nullptr;
		}
		if (!m_chunk) {
			TerrainChunkCreateInfo info;
			info.scale = m_size / TerrainChunk::SIZE;
			info.base_x = m_base_x;
			info.base_y = m_base_y;
			info.base_z = m_base_z;
			m_chunk = new TerrainChunk(info);
			loader.load(*m_chunk);
		}
		is_collapsed = true;
	}

	int64_t m_base_x, m_base_y, m_base_z, m_size;
	TerrainOctreeNode *m_children[8];
	TerrainChunk *m_chunk = nullptr;
	bool is_collapsed = true;
};

/*struct TerrainQuadtreeNode {
	TerrainQuadtreeNode(int64_t base_x, int64_t base_z, int64_t size)
		: m_base_x(base_x), m_base_z(base_z), m_size(size) {
		for (int i = 0; i < 4; i++)
			m_children_quad[i] = nullptr;
	}

	~TerrainQuadtreeNode() {
		if (m_next_octree) {
			for (int i = 0; i < 4; i++)
				delete m_children_oct[i];
		}
		else {
			for (int i = 0; i < 4; i++)
				delete m_children_quad[i];
		}
	}

	void updateChunks(double x, double y, double z, TerrainChunkGenerator &gen) {

	}

	void split(TerrainChunkGenerator &gen) {

	}

	void collapse(TerrainChunkGenerator &gen) {

	}

	int64_t m_base_x, m_base_z, m_size;
	union {
		TerrainQuadtreeNode *m_children_quad[4];
		TerrainOctreeNode *m_children_oct[4];
	};
	bool m_next_octree = false;
};*/

// --- TerrainOctree ---

inline bool isPowerOfTwo(uint32_t num) noexcept { return (num & (num - 1)) == 0; }

TerrainOctree::TerrainOctree(TerrainLoader &loader, uint32_t num_xz_chunks, uint32_t num_y_chunks)
   : m_xz_chunks(num_xz_chunks), m_y_chunks(num_y_chunks) {
	if (num_xz_chunks == 0 || !isPowerOfTwo(num_xz_chunks))
		throw MessageException("Number of chunks in XZ is not a power of two");
	if (num_y_chunks == 0 || !isPowerOfTwo(num_y_chunks))
		throw MessageException("Number of chunks in Y is not a power of two");
	if (num_xz_chunks <= num_y_chunks)
		throw MessageException("Number of chunks in XZ is less than or equal to Y");
	int64_t node_size = int64_t(TerrainChunk::SIZE) * int64_t(num_xz_chunks);
	int64_t base_coord = -node_size / 2;
	//m_tree = new TerrainQuadtreeNode(base_coord, base_coord, node_size);
	m_tree = new TerrainOctreeNode(base_coord, base_coord, base_coord, node_size, loader);
	Log::info("Created terrain octree with {} XZ and {} Y chunks", m_xz_chunks, m_y_chunks);
}

TerrainOctree::TerrainOctree(const TerrainOctree &other)
   : m_xz_chunks(other.m_xz_chunks), m_y_chunks(other.m_y_chunks) {
	if (other.m_tree)
		m_tree = new TerrainOctreeNode(*other.m_tree);
	else
		m_tree = nullptr;
}

TerrainOctree::~TerrainOctree() {
	delete m_tree;
}

void TerrainOctree::updateChunks(double x, double y, double z, TerrainLoader &loader) {
	m_tree->updateChunks(x, y, z, loader);
}

void TerrainOctree::walkActiveChunks(std::function<void(const TerrainChunk &)> visitor) const {
	if (!m_tree)
		return;

	std::vector<const TerrainOctreeNode *> stack;
	stack.emplace_back(m_tree);

	while (!stack.empty ()) {
		const TerrainOctreeNode *node = stack.back();
		stack.pop_back();

		if (node->is_collapsed) {
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
