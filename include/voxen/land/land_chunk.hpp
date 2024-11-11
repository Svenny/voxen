#pragma once

#include <voxen/land/compressed_chunk_storage.hpp>

namespace voxen::land
{

class Chunk {
public:
	using BlockId = uint16_t;

	using BlockIdStorage = CompressedChunkStorage<BlockId>;
	using BlockIdArray = CubeArray<BlockId, Consts::CHUNK_SIZE_BLOCKS>;

	void setAllBlocks(BlockIdStorage::ConstExpandedView view);
	void setAllBlocksUniform(BlockId value);

	const BlockIdStorage &blockIds() const noexcept { return m_block_ids; }

private:
	BlockIdStorage m_block_ids;
};

struct ChunkAdjacencyRef {
	constexpr static uint32_t SIZE = Consts::CHUNK_SIZE_BLOCKS + 2;

	const Chunk &chunk;
	const Chunk *adjacent[6];

	void expandBlockIds(CubeArrayView<Chunk::BlockId, SIZE> view) const;
};

} // namespace voxen::land
