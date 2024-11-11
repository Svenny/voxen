#include <voxen/land/land_state.hpp>

#include <voxen/common/v8g_hash_trie_impl.hpp>

namespace voxen
{

// Instantiate templates in this translation unit
template class V8gHashTrie<land::ChunkKey, land::Chunk>;
template class V8gHashTrie<land::ChunkKey, land::PseudoChunkData>;
template class V8gHashTrie<land::ChunkKey, land::PseudoChunkSurface>;

} // namespace voxen
