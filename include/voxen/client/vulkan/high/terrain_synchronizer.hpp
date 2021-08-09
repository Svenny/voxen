#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/client/vulkan/buffer.hpp>

#include <voxen/common/terrain/chunk.hpp>

#include <functional>
#include <unordered_map>
#include <vector>

namespace voxen::client::vulkan
{

struct TerrainChunkGpuData {
	uint32_t index_count;
	uint32_t first_index;
	int32_t vertex_offset;
};

class TerrainSynchronizer {
public:
	TerrainSynchronizer() = default;
	TerrainSynchronizer(TerrainSynchronizer &&) = delete;
	TerrainSynchronizer(const TerrainSynchronizer &) = delete;
	TerrainSynchronizer &operator = (TerrainSynchronizer &&) = delete;
	TerrainSynchronizer &operator = (const TerrainSynchronizer &) = delete;
	~TerrainSynchronizer() = default;

	void beginSyncSession();
	void syncChunk(const terrain::Chunk &chunk);
	void endSyncSession();

	void walkActiveChunks(std::function<void(terrain::ChunkId, const TerrainChunkGpuData &)> chunk_callback,
	                      std::function<void(VkBuffer, VkBuffer)> buffers_switch_callback);
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

	void enqueueSurfaceTransfer(const terrain::ChunkOwnSurface &own_surface,
	                            const terrain::ChunkSeamSurface &seam_surface, ChunkGpuData &data);
};

}
