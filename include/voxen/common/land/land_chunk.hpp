#pragma once

#include <voxen/common/land/land_chunk_storage.hpp>

namespace voxen::land
{

class Chunk {
public:
	using BlockIdStorage = CompressedChunkStorage<uint16_t>;

	void setAllBlocks(const BlockIdStorage::ExpandedArray &array);
	void setAllBlocksUniform(uint16_t value);

	const BlockIdStorage &blockIds() const noexcept { return m_block_ids; }

private:
	BlockIdStorage m_block_ids;
};

struct ChunkAdjacencyRef {
	constexpr static uint32_t SIZE = Consts::CHUNK_SIZE_BLOCKS + 2;

	const Chunk &chunk;
	const Chunk *adjacent[6];

	void expandBlockIds(CubeArrayView<uint16_t, SIZE> view) const;
};

} // namespace voxen::land
