#include <voxen/client/vulkan/high/terrain_synchronizer.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/high/transfer_manager.hpp>
#include <voxen/common/terrain/surface.hpp>

namespace voxen::client::vulkan
{

void TerrainSynchronizer::beginSyncSession()
{
	m_sync_age++;
}

void TerrainSynchronizer::syncChunk(const terrain::Chunk &chunk)
{
	const auto &id = chunk.id();
	const auto &own_surface = chunk.ownSurface();
	const auto &seam_surface = chunk.seamSurface();
	auto iter = m_chunk_gpu_data.find(id);

	VkDeviceSize needed_vtx_size = seam_surface.numAllVertices() * sizeof(terrain::SurfaceVertex);
	VkDeviceSize needed_idx_size = (own_surface.numIndices() + seam_surface.numIndices()) * sizeof(uint32_t);

	if (needed_vtx_size == 0 || needed_idx_size == 0) {
		// No mesh, erase any data and return immediately to avoid empty allocations
		if (iter != m_chunk_gpu_data.end())
			m_chunk_gpu_data.erase(iter);
		return;
	}

	if (iter != m_chunk_gpu_data.end()) {
		auto &data = iter->second;
		// Update age in any case
		data.age = m_sync_age;

		if (data.version == chunk.version() && data.seam_version == chunk.seamVersion()) {
			// Already synchronized
			return;
		}

		if (data.vtx_buffer.size() >= needed_vtx_size && data.idx_buffer.size() >= needed_idx_size) {
			// Enough space in buffers, just enqueue a transfer
			data.version = chunk.version();
			data.seam_version = chunk.seamVersion();
			enqueueSurfaceTransfer(own_surface, seam_surface, iter->second);
			return;
		}

		// No space in buffers, need to create new ones. Proceed as if adding a new entry
		m_chunk_gpu_data.erase(iter);
	}

	VkBufferCreateInfo vtx_info = {};
	vtx_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vtx_info.size = needed_vtx_size;
	vtx_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	vtx_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkBufferCreateInfo idx_info = {};
	idx_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	idx_info.size = needed_idx_size;
	idx_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	idx_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	// Add a new entry
	iter = m_chunk_gpu_data.emplace(id, ChunkGpuData {
		.vtx_buffer = Buffer(vtx_info, Buffer::Usage::DeviceLocal),
		.idx_buffer = Buffer(idx_info, Buffer::Usage::DeviceLocal),
		.index_count = own_surface.numIndices() + seam_surface.numIndices(),
		.version = chunk.version(),
		.seam_version = chunk.seamVersion(),
		.age = m_sync_age
	}).first;

	enqueueSurfaceTransfer(own_surface, seam_surface, iter->second);
}

void TerrainSynchronizer::endSyncSession()
{
	Backend::backend().transferManager().ensureUploadsDone();
}

void TerrainSynchronizer::walkActiveChunks(std::function<void(terrain::ChunkId, const TerrainChunkGpuData &)> chunk_callback,
                                           std::function<void(VkBuffer, VkBuffer)> buffers_switch_callback)
{
	// TODO: replace with framerate-independent garbage collection mechanism
	constexpr uint32_t MAX_AGE_DIFF = 720; // 720 frames (5 sec on 144 FPS, 12 sec on 60 FPS)

	TerrainChunkGpuData data = {};
	std::vector<terrain::ChunkId> for_erase;

	for (const auto &entry : m_chunk_gpu_data) {
		if (entry.second.age == m_sync_age) {
			buffers_switch_callback(entry.second.vtx_buffer, entry.second.idx_buffer);
			data.index_count = entry.second.index_count;
			chunk_callback(entry.first, data);
		}

		uint32_t age_diff = m_sync_age - entry.second.age;
		if (age_diff > MAX_AGE_DIFF)
			for_erase.emplace_back(entry.first);
	}

	for (const auto &id : for_erase)
		m_chunk_gpu_data.erase(id);
}

void TerrainSynchronizer::enqueueSurfaceTransfer(const terrain::ChunkOwnSurface &own_surface,
                                                 const terrain::ChunkSeamSurface &seam_surface, ChunkGpuData &data)
{
	auto &transfer = Backend::backend().transferManager();

	const VkDeviceSize own_vertices_size = own_surface.numVertices() * sizeof(terrain::SurfaceVertex);
	const VkDeviceSize own_indices_size = own_surface.numIndices() * sizeof(uint32_t);
	const VkDeviceSize seam_vertices_size = seam_surface.numExtraVertices() * sizeof(terrain::SurfaceVertex);
	const VkDeviceSize seam_indices_size = seam_surface.numIndices() * sizeof(uint32_t);

	// TODO (Svenny): support split own/seam surface updates.
	// Currently a full reupload is done even if only the seam has changed.
	if (own_vertices_size > 0) {
		transfer.uploadToBuffer(data.vtx_buffer, 0, own_surface.vertices(), own_vertices_size);
	}
	if (seam_vertices_size > 0) {
		transfer.uploadToBuffer(data.vtx_buffer, own_vertices_size, seam_surface.extraVertices(), seam_vertices_size);
	}

	if (own_indices_size > 0) {
		transfer.uploadToBuffer(data.idx_buffer, 0, own_surface.indices(), own_indices_size);
	}
	if (seam_indices_size > 0) {
		transfer.uploadToBuffer(data.idx_buffer, own_indices_size, seam_surface.indices(), seam_indices_size);
	}
}

}
