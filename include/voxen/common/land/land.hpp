#pragma once

#include <voxen/common/land/chunk_key.hpp>
#include <voxen/common/land/fake_chunk_data.hpp>
#include <voxen/common/land/land_chunk.hpp>
#include <voxen/common/v8g_flat_map.hpp>
#include <voxen/common/v8g_hash_trie.hpp>
#include <voxen/common/v8g_helpers.hpp>

#include <glm/glm.hpp>

#include <memory>

namespace voxen
{

// Declare extern template instantiations (defined in land.cpp)
extern template class V8gHashTrie<land::ChunkKey, land::Chunk>;
extern template class V8gHashTrie<land::ChunkKey, land::FakeChunkData>;

} // namespace voxen

namespace voxen::land
{

class BlockRegistry;

struct LandState {
	using ChunkTable = V8gHashTrie<ChunkKey, Chunk>;
	using FakeChunkDataTable = V8gHashTrie<ChunkKey, FakeChunkData>;

	ChunkTable chunk_table;
	FakeChunkDataTable fake_chunk_data_table;
};

class Land {
public:
	Land();
	Land(Land &&) = delete;
	Land(const Land &) = delete;
	Land &operator=(Land &&) = delete;
	Land &operator=(const Land &) = delete;
	~Land() noexcept;

	// TODO: initially deprecated, should be replaced with messaging
	void setLoadingPoint(glm::dvec3 point);
	void tick();

	BlockRegistry &blockRegsitry() noexcept;

	const LandState &stateForCopy() const noexcept;

private:
	struct Impl;

	std::unique_ptr<Impl> m_impl;
};

} // namespace voxen::land
