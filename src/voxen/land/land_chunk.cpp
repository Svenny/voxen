#include <voxen/land/land_chunk.hpp>

namespace voxen::land
{

void Chunk::setAllBlocks(BlockIdStorage::ConstExpandedView view)
{
	m_block_ids = BlockIdStorage(view);
}

void Chunk::setAllBlocksUniform(BlockId value)
{
	m_block_ids.setUniform(value);
}

void ChunkAdjacencyRef::expandBlockIds(CubeArrayView<Chunk::BlockId, SIZE> view) const
{
	constexpr static uint32_t N = Consts::CHUNK_SIZE_BLOCKS;

	// Clear everything to zeros (second data pass but greatly simplifies the code)
	view.fill(0);

	// Fill the main part (always available)
	chunk.blockIds().expand(view.view<N>(glm::uvec3(1)));

	if (adjacent[0]) { // X+
		const auto &ids = adjacent[0]->blockIds();

		for (uint32_t y = 0; y < N; y++) {
			for (uint32_t z = 0; z < N; z++) {
				// Load from west, store to east
				view.store(N + 1, y + 1, z + 1, ids.load(0, y, z));
			}
		}
	}

	if (adjacent[1]) { // X-
		const auto &ids = adjacent[1]->blockIds();

		for (uint32_t y = 0; y < N; y++) {
			for (uint32_t z = 0; z < N; z++) {
				// Load from east, store to west
				view.store(0, y + 1, z + 1, ids.load(N - 1, y, z));
			}
		}
	}

	if (adjacent[2]) { // Y+
		const auto &ids = adjacent[2]->blockIds();

		for (uint32_t x = 0; x < N; x++) {
			for (uint32_t z = 0; z < N; z++) {
				// Load from bottom, store to top
				view.store(x + 1, N + 1, z + 1, ids.load(x, 0, z));
			}
		}
	}

	if (adjacent[3]) { // Y-
		const auto &ids = adjacent[3]->blockIds();

		for (uint32_t x = 0; x < N; x++) {
			for (uint32_t z = 0; z < N; z++) {
				// Load from top, store to bottom
				view.store(x + 1, 0, z + 1, ids.load(x, N - 1, z));
			}
		}
	}

	if (adjacent[4]) { // Z+
		const auto &ids = adjacent[4]->blockIds();

		for (uint32_t y = 0; y < N; y++) {
			for (uint32_t x = 0; x < N; x++) {
				// Load from north, store to south
				view.store(x + 1, y + 1, N + 1, ids.load(x, y, 0));
			}
		}
	}

	if (adjacent[5]) { // Z-
		const auto &ids = adjacent[5]->blockIds();

		for (uint32_t y = 0; y < N; y++) {
			for (uint32_t x = 0; x < N; x++) {
				// Load from south, store to north
				view.store(x + 1, y + 1, 0, ids.load(x, y, N - 1));
			}
		}
	}
}

} // namespace voxen::land
