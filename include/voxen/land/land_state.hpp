#pragma once

#include <voxen/common/v8g_hash_trie.hpp>
#include <voxen/land/chunk_key.hpp>
#include <voxen/land/land_chunk.hpp>
#include <voxen/land/pseudo_chunk_data.hpp>
#include <voxen/land/pseudo_chunk_surface.hpp>

namespace voxen
{

// Declare extern template instantiations (defined in cpp file)
extern template class V8gHashTrie<land::ChunkKey, land::Chunk>;
extern template class V8gHashTrie<land::ChunkKey, land::PseudoChunkData>;
extern template class V8gHashTrie<land::ChunkKey, land::PseudoChunkSurface>;

} // namespace voxen

namespace voxen::land
{

struct LandState {
	using ChunkTable = V8gHashTrie<ChunkKey, Chunk>;
	using PseudoChunkDataTable = V8gHashTrie<ChunkKey, PseudoChunkData>;
	using PseudoChunkSurfaceTable = V8gHashTrie<ChunkKey, PseudoChunkSurface>;

	ChunkTable chunk_table;
	PseudoChunkDataTable pseudo_chunk_data_table;
	PseudoChunkSurfaceTable pseudo_chunk_surface_table;
};

} // namespace voxen::land
