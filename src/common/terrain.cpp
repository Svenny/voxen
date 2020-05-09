#include <voxen/common/terrain.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include <algorithm>
#include <queue>

namespace voxen
{

TerrainChunk::TerrainChunk(uint32_t size, uint32_t scale, int64_t base_x, int64_t base_y, int64_t base_z)
   : m_size(size), m_scale(scale), m_base_x(base_x), m_base_y(base_y), m_base_z(base_z) {}

bool TerrainChunk::operator==(const TerrainChunk &other) const noexcept {
	return size() == other.size() && scale() == other.scale()
	      && baseX() == other.baseX() && baseY() == other.baseY() && baseZ() == other.baseZ();
}

// --- TerrainChunkCache ---

struct TerrainChunkCache {
public:
	bool find(const TerrainChunk &chunk) noexcept {
		size_t set_id = size_t(chunkHash(chunk)) % k_num_sets;
		size_t pos = k_set_size;
		for (size_t i = 0; i < k_set_size; i++) {
			if (entries[set_id][i].chunk && *(entries[set_id][i].chunk) == chunk) {
				pos = i;
				break;
			}
		}
		if (pos == k_set_size)
			return false;
		// Update positions - entires[set_id][pos] should become the last element
		std::rotate(&entries[set_id][pos], &entries[set_id][pos + 1], &entries[set_id][k_set_size]);
		return true;
	}

	void insert(const TerrainChunk &chunk) {
		size_t set_id = size_t(chunkHash(chunk)) % k_num_sets;
		// TODO: check if this chunk is already in the cache?
		// Replace the first element with our chunk
		delete entries[set_id][0].chunk;
		entries[set_id][0].chunk = new TerrainChunk(chunk);
		// And update positions - make it the last element
		std::rotate(&entries[set_id][0], &entries[set_id][1], &entries[set_id][k_set_size]);
	}
private:
	constexpr static size_t k_set_size = 32;
	constexpr static size_t k_num_sets = 128;

	struct CacheEntry {
		TerrainChunk *chunk = nullptr;
	};

	CacheEntry entries[k_num_sets][k_set_size];

	static uint64_t chunkHash(const TerrainChunk &chunk) noexcept {
		union {
			uint64_t data[4];
			uint8_t bytes[32];
		};
		data[0] = (uint64_t(chunk.size()) << 32) | uint64_t(chunk.scale());
		data[1] = static_cast<uint64_t>(chunk.baseX());
		data[2] = static_cast<uint64_t>(chunk.baseY());
		data[3] = static_cast<uint64_t>(chunk.baseZ());
		// FNV-1a
		uint64_t result = 0xCBF29CE484222325;
		for (int i = 0; i < 32; i++) {
			result *= 0x100000001B3;
			result ^= uint64_t(bytes[i]);
		}
		return result;
	}
};

// --- TerrainChunkGenerator ---

TerrainChunkGenerator::TerrainChunkGenerator() : m_cache(new TerrainChunkCache) {}

// TODO: copy cache state?
TerrainChunkGenerator::TerrainChunkGenerator(const TerrainChunkGenerator &other) : m_cache(new TerrainChunkCache) {
	(void)other;
}

TerrainChunkGenerator::~TerrainChunkGenerator() {
	delete m_cache;
}

void TerrainChunkGenerator::generate(TerrainChunk &chunk) {
	if (m_cache->find(chunk)) {
		//Log::debug()("Chunk ({}, {}, {}) [{}x{}] was loaded from standby cache",
		//             chunk.baseX(), chunk.baseY(), chunk.baseZ(), chunk.size(), chunk.scale());
	}
	else {
		m_cache->insert(chunk);
		//Log::debug()("Chunk ({}, {}, {}) [{}x{}] was generated / loaded from disk",
		//             chunk.baseX(), chunk.baseY(), chunk.baseZ(), chunk.size(), chunk.scale());
	}
}

// --- TerrainOctree nodes ---

constexpr int32_t k_chunk_size = 32;

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
	TerrainOctreeNode(int64_t base_x, int64_t base_y, int64_t base_z, int64_t size, TerrainChunkGenerator &gen)
	   : m_base_x(base_x), m_base_y(base_y), m_base_z(base_z), m_size(size) {
		for (int i = 0; i < 8; i++)
			m_children[i] = nullptr;
		m_chunk = new TerrainChunk(k_chunk_size, m_size / k_chunk_size, m_base_x, m_base_y, m_base_z);
		gen.generate(*m_chunk);
	}

	TerrainOctreeNode(const TerrainOctreeNode &other)
	   : m_base_x(other.m_base_x), m_base_y(other.m_base_y), m_base_z(other.m_base_z), m_size(other.m_size) {
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

	void updateChunks(double x, double y, double z, TerrainChunkGenerator &gen) {
		if (m_size == k_chunk_size)
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
			split(gen);
			for (int i = 0; i < 8; i++)
				if (m_children[i])
					m_children[i]->updateChunks(x, y, z, gen);
		}
		else if (dist > 3 * m_size) {
			//if (!is_collapsed)
			//	Log::debug()("Collapsing octree node ({}, {}, {}) [{}]", m_base_x, m_base_y, m_base_z, m_size);
			collapse(gen);
		}
	}

	void split(TerrainChunkGenerator &gen) {
		if (!is_collapsed)
			return;
		delete m_chunk;
		m_chunk = nullptr;
		int64_t child_size = m_size / 2;
		for (int i = 0; i < 8; i++) {
			if (!m_children[i]) {
				int64_t base_x = m_base_x + child_size * k_child_offset[i][0];
				int64_t base_y = m_base_y + child_size * k_child_offset[i][1];
				int64_t base_z = m_base_z + child_size * k_child_offset[i][2];
				m_children[i] = new TerrainOctreeNode(base_x, base_y, base_z, child_size, gen);
			}
		}
		is_collapsed = false;
	}

	void collapse(TerrainChunkGenerator &gen) {
		if (is_collapsed)
			return;
		for (int i = 0; i < 8; i++) {
			delete m_children[i];
			m_children[i] = nullptr;
		}
		if (!m_chunk) {
			m_chunk = new TerrainChunk(k_chunk_size, m_size / k_chunk_size, m_base_x, m_base_y, m_base_z);
			gen.generate(*m_chunk);
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

TerrainOctree::TerrainOctree(uint32_t num_xz_chunks, uint32_t num_y_chunks)
   : m_xz_chunks(num_xz_chunks), m_y_chunks(num_y_chunks) {
	if (num_xz_chunks == 0 || !isPowerOfTwo(num_xz_chunks))
		throw MessageException("Number of chunks in XZ is not a power of two");
	if (num_y_chunks == 0 || !isPowerOfTwo(num_y_chunks))
		throw MessageException("Number of chunks in Y is not a power of two");
	if (num_xz_chunks <= num_y_chunks)
		throw MessageException("Number of chunks in XZ is less than or equal to Y");
	int64_t node_size = k_chunk_size * int64_t(num_xz_chunks);
	int64_t base_coord = -node_size / 2;
	//m_tree = new TerrainQuadtreeNode(base_coord, base_coord, node_size);
	m_tree = new TerrainOctreeNode(base_coord, base_coord, base_coord, node_size, m_chunk_gen);
	Log::info("Created terrain octree with {} XZ and {} Y chunks", m_xz_chunks, m_y_chunks);
}

TerrainOctree::TerrainOctree(const TerrainOctree &other)
   : m_xz_chunks(other.m_xz_chunks), m_y_chunks(other.m_y_chunks), m_chunk_gen(other.m_chunk_gen) {
	if (other.m_tree)
		m_tree = new TerrainOctreeNode(*other.m_tree);
	else
		m_tree = nullptr;
}

TerrainOctree::~TerrainOctree() {
	delete m_tree;
}

void TerrainOctree::updateChunks(double x, double y, double z) {
	m_tree->updateChunks(x, y, z, m_chunk_gen);
}

void TerrainOctree::walkActiveChunks(std::function<void(const TerrainChunk &)> visitor) const {
	std::queue<const TerrainOctreeNode *> q;
	q.emplace(m_tree);
	while (!q.empty ()) {
		const TerrainOctreeNode *node = q.front();
		q.pop();
		TerrainChunk *chunk = node->m_chunk;
		if(chunk)
			visitor(*chunk);
		for (size_t i = 0; i < 8; i++)
			if(node->m_children[i])
				q.push(node->m_children[i]);
	}
}

}
