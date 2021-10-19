#include <voxen/client/vulkan/high/terrain_synchronizer.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/config.hpp>
#include <voxen/client/vulkan/physical_device.hpp>
#include <voxen/client/vulkan/high/transfer_manager.hpp>
#include <voxen/common/terrain/surface.hpp>
#include <voxen/util/log.hpp>

#include <extras/linear_allocator.hpp>

namespace voxen::client::vulkan
{

constexpr static VkDeviceSize VERTEX_ARENA_SIZE = sizeof(terrain::SurfaceVertex) * Config::MAX_TERRAIN_ARENA_VERTICES;
constexpr static VkDeviceSize INDEX_ARENA_SIZE = sizeof(uint16_t) * Config::MAX_TERRAIN_ARENA_INDICES;
constexpr static uint32_t ALIGNMENT = static_cast<uint32_t>(Config::TERRAIN_SUBALLOCATION_ALIGNMENT);

class TerrainDataArena final : private extras::linear_allocator<TerrainDataArena, uint32_t, ALIGNMENT> {
public:
	using Base = extras::linear_allocator<TerrainDataArena, uint32_t, ALIGNMENT>;
	using Range = std::pair<uint32_t, uint32_t>;

	explicit TerrainDataArena(const VkBufferCreateInfo &info) : Base(static_cast<uint32_t>(info.size)),
		m_buffer(info, DeviceMemoryUseCase::GpuOnly)
	{}

	TerrainDataArena(TerrainDataArena &&) = delete;
	TerrainDataArena(const TerrainDataArena &) = delete;
	TerrainDataArena &operator = (TerrainDataArena &&) = delete;
	TerrainDataArena &operator = (const TerrainDataArena &) = delete;
	~TerrainDataArena() = default;

	bool tryHostMap()
	{
		return !!m_buffer.allocation().tryHostMap();
	}

	[[nodiscard]] std::optional<Range> allocate(uint32_t size)
	{
		return Base::allocate(size, ALIGNMENT);
	}

	static void on_allocator_freed(Base &base) noexcept
	{
		Backend::backend().terrainSynchronizer().arenaFreeCallback(static_cast<TerrainDataArena *>(&base));
	}

private:
	FatVkBuffer m_buffer;
};

TerrainSynchronizer::TerrainSynchronizer()
{
	auto &dev = Backend::backend().physicalDevice();
	m_queue_families[0] = dev.graphicsQueueFamily();
	m_queue_families[1] = dev.transferQueueFamily();

	if (m_queue_families[0] != m_queue_families[1]) {
		Log::info("GPU has DMA queue, surface transfers will go through it");
	}

	Log::debug("TerrainSynchronizer created successfully");
}

TerrainSynchronizer::~TerrainSynchronizer() noexcept = default;

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
			data.index_count = own_surface.numIndices() + seam_surface.numIndices();
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
		.vtx_buffer = FatVkBuffer(vtx_info, DeviceMemoryUseCase::GpuOnly),
		.idx_buffer = FatVkBuffer(idx_info, DeviceMemoryUseCase::GpuOnly),
		.index_count = own_surface.numIndices() + seam_surface.numIndices(),
		.version = chunk.version(),
		.seam_version = chunk.seamVersion(),
		.age = m_sync_age
	}).first;

	enqueueSurfaceTransfer(own_surface, seam_surface, iter->second);
}

TerrainSynchronizer::ChunkRenderInfo TerrainSynchronizer::syncChunk(const extras::refcnt_ptr<terrain::Chunk> &chunk)
{
	syncChunk(*chunk);

	auto iter = m_chunk_gpu_data.find(chunk->id());
	return {
		.vertex_buffer = iter->second.vtx_buffer.handle(),
		.index_buffer = iter->second.idx_buffer.handle(),
		.index_type = VK_INDEX_TYPE_UINT32,
		.first_vertex = 0,
		.first_index = 0,
		.num_vertices = chunk->seamSurface().numAllVertices(),
		.num_indices = iter->second.index_count
	};
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

void TerrainSynchronizer::arenaFreeCallback(TerrainDataArena *arena) noexcept
{
	assert(arena);

	for (auto iter = m_vertex_arenas.begin(); iter != m_vertex_arenas.end(); ++iter) {
		if (&(*iter) == arena) {
			m_vertex_arenas.erase(iter);
			return;
		}
	}

	for (auto iter = m_index_arenas.begin(); iter != m_index_arenas.end(); ++iter) {
		if (&(*iter) == arena) {
			m_index_arenas.erase(iter);
			return;
		}
	}

	// Arena must be in one of the lists
	assert(false);
	__builtin_unreachable();
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

void TerrainSynchronizer::addVertexArena()
{
	const bool has_dma = m_queue_families[0] != m_queue_families[1];

	const VkBufferCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.size = VERTEX_ARENA_SIZE,
		.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.sharingMode = has_dma ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 2,
		.pQueueFamilyIndices = m_queue_families
	};
	m_vertex_arenas.emplace_back(info);
}

void TerrainSynchronizer::addIndexArena()
{
	const bool has_dma = m_queue_families[0] != m_queue_families[1];

	const VkBufferCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.size = INDEX_ARENA_SIZE,
		.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		.sharingMode = has_dma ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 2,
		.pQueueFamilyIndices = m_queue_families
	};
	m_index_arenas.emplace_back(info);
}

}
