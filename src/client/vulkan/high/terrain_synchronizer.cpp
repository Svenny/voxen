#include <voxen/client/vulkan/high/terrain_synchronizer.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/high/transfer_manager.hpp>

namespace voxen::client::vulkan
{

void TerrainSynchronizer::beginSyncSession()
{
	m_sync_age++;
}

void TerrainSynchronizer::syncChunk(const TerrainChunk &chunk)
{
	const auto &header = chunk.header();
	const auto &surface = chunk.data().surface;
	auto iter = m_chunk_gpu_data.find(header);

	VkDeviceSize needed_vtx_size = surface.numVertices() * sizeof(TerrainSurfaceVertex);
	VkDeviceSize needed_idx_size = surface.numIndices() * sizeof(uint32_t);

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

		if (data.version == chunk.version()) {
			// Already synchronized
			return;
		}

		if (data.vtx_buffer.size() >= needed_vtx_size && data.idx_buffer.size() >= needed_idx_size) {
			// Enough space in buffers, just enqueue a transfer
			data.version = chunk.version();
			enqueueSurfaceTransfer(surface, iter->second);
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
	iter = m_chunk_gpu_data.emplace(header, ChunkGpuData {
		.vtx_buffer = Buffer(vtx_info, Buffer::Usage::DeviceLocal),
		.idx_buffer = Buffer(idx_info, Buffer::Usage::DeviceLocal),
		.index_count = surface.numIndices(),
		.version = chunk.version(),
		.age = m_sync_age
	}).first;

	enqueueSurfaceTransfer(surface, iter->second);
}

void TerrainSynchronizer::endSyncSession()
{
	Backend::backend().transferManager()->ensureUploadsDone();
}

void TerrainSynchronizer::walkActiveChunks(std::function<void(const TerrainChunkGpuData &)> chunk_callback,
                                           std::function<void(VkBuffer, VkBuffer)> buffers_switch_callback)
{
	// TODO: replace with framerate-independent garbage collection mechanism
	constexpr uint32_t MAX_AGE_DIFF = 720; // 720 frames (5 sec on 144 FPS, 12 sec on 60 FPS)

	TerrainChunkGpuData data = {};
	std::vector<TerrainChunkHeader> for_erase;

	for (const auto &entry : m_chunk_gpu_data) {
		if (entry.second.age == m_sync_age) {
			buffers_switch_callback(entry.second.vtx_buffer, entry.second.idx_buffer);
			data.header = entry.first;
			data.index_count = entry.second.index_count;
			chunk_callback(data);
		}

		uint32_t age_diff = m_sync_age - entry.second.age;
		if (age_diff > MAX_AGE_DIFF)
			for_erase.emplace_back(entry.first);
	}

	for (const auto &header : for_erase)
		m_chunk_gpu_data.erase(header);
}

void TerrainSynchronizer::enqueueSurfaceTransfer(const TerrainSurface &surface, ChunkGpuData &data)
{
	auto *transfer = Backend::backend().transferManager();
	transfer->uploadToBuffer(data.vtx_buffer, surface.vertices(), surface.numVertices() * sizeof(TerrainSurfaceVertex));
	transfer->uploadToBuffer(data.idx_buffer, surface.indices(), surface.numIndices() * sizeof(uint32_t));
}

}
