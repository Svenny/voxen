#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/client/vulkan/buffer.hpp>

#include <voxen/common/terrain/chunk.hpp>

#include <functional>
#include <list>
#include <unordered_map>

namespace voxen::client::vulkan
{

struct TerrainChunkGpuData {
	uint32_t index_count;
	uint32_t first_index;
	int32_t vertex_offset;
};

// NOTE: this class is internal to `TerrainSynchronizer` implementation and is not used in any public API
class TerrainDataArena;

class TerrainSynchronizer {
public:
	TerrainSynchronizer();
	TerrainSynchronizer(TerrainSynchronizer &&) = delete;
	TerrainSynchronizer(const TerrainSynchronizer &) = delete;
	TerrainSynchronizer &operator = (TerrainSynchronizer &&) = delete;
	TerrainSynchronizer &operator = (const TerrainSynchronizer &) = delete;
	~TerrainSynchronizer() noexcept;

	void beginSyncSession();
	void syncChunk(const terrain::Chunk &chunk);
	void endSyncSession();

	void walkActiveChunks(std::function<void(terrain::ChunkId, const TerrainChunkGpuData &)> chunk_callback,
	                      std::function<void(VkBuffer, VkBuffer)> buffers_switch_callback);

	// This method is called by `TerrainDataArena` when it is completely free.
	// Arena is deleted after calling this, so in some sense it's similar to `delete this`.
	// NOTE: this is an internal API exposed in public section, do not call it.
	void arenaFreeCallback(TerrainDataArena *arena) noexcept;

private:
	struct ChunkGpuData {
		FatVkBuffer vtx_buffer;
		FatVkBuffer idx_buffer;
		uint32_t index_count;
		uint32_t version;
		uint32_t seam_version;
		uint32_t age;
	};

	std::unordered_map<terrain::ChunkId, ChunkGpuData> m_chunk_gpu_data;
	uint32_t m_sync_age = 0;

	uint32_t m_queue_families[2];
	std::list<TerrainDataArena> m_vertex_arenas;
	std::list<TerrainDataArena> m_index_arenas;

	void enqueueSurfaceTransfer(const terrain::ChunkOwnSurface &own_surface,
	                            const terrain::ChunkSeamSurface &seam_surface, ChunkGpuData &data);

	void addVertexArena();
	void addIndexArena();
};

}
