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

// NOTE: alignment must evenly divide vertex size, otherwise fractional (thus impossible) vertex offsets can happen
constexpr static uint32_t ALIGNMENT = 4;
static_assert(sizeof(terrain::SurfaceVertex) % ALIGNMENT == 0);

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

	void *hostPointer() const noexcept
	{
		return m_buffer.allocation().hostPointer();
	}

	[[nodiscard]] std::optional<Range> allocate(uint32_t size)
	{
		return Base::allocate(size, ALIGNMENT);
	}

	void free(Range range) noexcept
	{
		Base::free(range);
	}

	VkBuffer bufferHandle() const noexcept { return m_buffer.handle(); }

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

	// Add two arenas immediately to set UMA flags
	addVertexArena();
	addIndexArena();

	Log::debug("TerrainSynchronizer created successfully");
}

TerrainSynchronizer::~TerrainSynchronizer() noexcept = default;

void TerrainSynchronizer::beginSyncSession()
{
	m_sync_age++;
}

TerrainSynchronizer::ChunkRenderInfo TerrainSynchronizer::syncChunk(const extras::refcnt_ptr<terrain::Chunk> &chunk)
{
	// TODO (Svenny): this implementation has the following issues:
	// - Uploading is synchronous for both UMA and NUMA modes (`loading_request_id` is a stub for this)
	// - Loading slot is unused - there is a race between outstanding rendering and uploading for the next frame
	// - No U32 -> U16/U8 index type narrowing when possible (should this be done at chunk contouring time?)
	// - Almost no data reusing attempts (?)
	// - `slot_switch_age` is used more like `sync_age` (though because of p.2)
	// - Going through arenas list every time to obtain reference/handle is not efficient
	const auto &id = chunk->id();
	const auto &own_surface = chunk->ownSurface();
	const auto &seam_surface = chunk->seamSurface();

	auto &data = m_per_chunk_data[id];
	// Update age in any case
	data.slot_switch_age = m_sync_age;

	if (data.last_chunk && data.last_chunk->version() == chunk->version() &&
	    data.last_chunk->seamVersion() == chunk->seamVersion()) {
		// Chunk does not need updating
		return slotToRenderInfo(data.slot_active);
	}

	data.last_chunk = chunk;

	const uint32_t num_vertices = seam_surface.numAllVertices();
	const uint32_t num_indices = own_surface.numIndices() + seam_surface.numIndices();

	VkDeviceSize needed_vtx_size = num_vertices * sizeof(terrain::SurfaceVertex);
	VkDeviceSize needed_idx_size = num_indices * sizeof(uint32_t);
	assert(needed_vtx_size > 0 && needed_idx_size > 0);

	auto &slot = data.slot_active;
	if (slot.vertex_arena_range < needed_vtx_size || slot.index_arena_range < needed_idx_size) {
		// Not enough space in buffers, reallocate a bigger storage
		clearSlot(slot);
		allocateSlot(slot, needed_vtx_size, needed_idx_size);
	}

	slot.index_type = VK_INDEX_TYPE_UINT32;
	slot.num_vertices = num_vertices;
	slot.num_indices = num_indices;

	makeSurfaceTransfer(slot, *chunk);
	return slotToRenderInfo(slot);
}

void TerrainSynchronizer::endSyncSession()
{
	Backend::backend().transferManager().ensureUploadsDone();
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

void TerrainSynchronizer::clearSlot(ChunkSlotSyncData &slot) noexcept
{
	if (slot.vertex_arena_id != UINT16_MAX) {
		std::pair<uint32_t, uint32_t> range(slot.vertex_arena_offset, slot.vertex_arena_offset + slot.vertex_arena_range);
		selectArena(m_vertex_arenas, slot.vertex_arena_id).free(range);
		slot.vertex_arena_offset = 0;
		slot.vertex_arena_range = 0;
	}
	slot.vertex_arena_id = UINT16_MAX;

	if (slot.index_arena_id != UINT16_MAX) {
		std::pair<uint32_t, uint32_t> range(slot.index_arena_offset, slot.index_arena_offset + slot.index_arena_range);
		selectArena(m_index_arenas, slot.index_arena_id).free(range);
		slot.index_arena_offset = 0;
		slot.index_arena_range = 0;
	}
	slot.index_arena_id = UINT16_MAX;
}

void TerrainSynchronizer::allocateSlot(ChunkSlotSyncData &slot, VkDeviceSize vtx_size, VkDeviceSize idx_size)
{
	uint16_t vertex_arena_id = 0;
	bool found_vertex_arena = false;

	for (auto iter = m_vertex_arenas.begin(); iter != m_vertex_arenas.end(); ++iter) {
		auto result = iter->allocate(vtx_size);
		if (result) {
			slot.vertex_arena_id = vertex_arena_id;
			slot.vertex_arena_offset = result->first;
			slot.vertex_arena_range = result->second - result->first;
			found_vertex_arena = true;
			break;
		}
		vertex_arena_id++;
	}

	if (!found_vertex_arena) {
		addVertexArena();
		Log::debug("Vertex arenas are full, adding new one ({} now)", m_vertex_arenas.size());

		auto result = m_vertex_arenas.back().allocate(vtx_size);
		// Can't fail on newly created arena
		assert(result.has_value());
		slot.vertex_arena_id = vertex_arena_id;
		slot.vertex_arena_offset = result->first;
		slot.vertex_arena_range = result->second - result->first;
	}

	uint16_t index_arena_id = 0;
	bool found_index_arena = false;

	for (auto iter = m_index_arenas.begin(); iter != m_index_arenas.end(); ++iter) {
		auto result = iter->allocate(idx_size);
		if (result) {
			slot.index_arena_id = index_arena_id;
			slot.index_arena_offset = result->first;
			slot.index_arena_range = result->second - result->first;
			found_index_arena = true;
			break;
		}
		index_arena_id++;
	}

	if (!found_index_arena) {
		addIndexArena();
		Log::debug("Index arenas are full, adding new one ({} now)", m_index_arenas.size());

		auto result = m_index_arenas.back().allocate(idx_size);
		// Can't fail on newly created arena
		assert(result.has_value());
		slot.index_arena_id = index_arena_id;
		slot.index_arena_offset = result->first;
		slot.index_arena_range = result->second - result->first;
	}
}

void TerrainSynchronizer::makeSurfaceTransfer(ChunkSlotSyncData &slot, const terrain::Chunk &chunk)
{
	auto &transfer = Backend::backend().transferManager();
	const auto &own_surface = chunk.ownSurface();
	const auto &seam_surface = chunk.seamSurface();

	const VkDeviceSize own_vtx_size = own_surface.numVertices() * sizeof(terrain::SurfaceVertex);
	const VkDeviceSize own_idx_size = own_surface.numIndices() * sizeof(uint32_t);
	const VkDeviceSize seam_vtx_size = seam_surface.numExtraVertices() * sizeof(terrain::SurfaceVertex);
	const VkDeviceSize seam_idx_size = seam_surface.numIndices() * sizeof(uint32_t);

	// TODO (Svenny): obtaining arenas this way is not particularly efficient
	auto &vtx_arena = selectArena(m_vertex_arenas, slot.vertex_arena_id);
	VkDeviceSize vtx_offset = slot.vertex_arena_offset;
	if (m_vertex_uma) {
		std::byte *vtx_buffer = reinterpret_cast<std::byte *>(vtx_arena.hostPointer());
		vtx_buffer += vtx_offset;
		memcpy(vtx_buffer, own_surface.vertices(), own_vtx_size);
		vtx_buffer += own_vtx_size;
		memcpy(vtx_buffer, seam_surface.extraVertices(), seam_vtx_size);
	} else {
		VkBuffer vtx_buffer = vtx_arena.bufferHandle();
		if (own_vtx_size > 0) {
			transfer.uploadToBuffer(vtx_buffer, vtx_offset, own_surface.vertices(), own_vtx_size);
		}
		if (seam_vtx_size > 0) {
			transfer.uploadToBuffer(vtx_buffer, vtx_offset + own_vtx_size, seam_surface.extraVertices(), seam_vtx_size);
		}
	}

	auto &idx_arena = selectArena(m_index_arenas, slot.index_arena_id);
	VkDeviceSize idx_offset = slot.index_arena_offset;
	if (m_index_uma) {
		std::byte *idx_buffer = reinterpret_cast<std::byte *>(idx_arena.hostPointer());
		idx_buffer += idx_offset;
		memcpy(idx_buffer, own_surface.indices(), own_idx_size);
		idx_buffer += own_idx_size;
		memcpy(idx_buffer, seam_surface.indices(), seam_idx_size);
	} else {
		VkBuffer idx_buffer = idx_arena.bufferHandle();
		if (own_idx_size > 0) {
			transfer.uploadToBuffer(idx_buffer, idx_offset, own_surface.indices(), own_idx_size);
		}
		if (seam_idx_size > 0) {
			transfer.uploadToBuffer(idx_buffer, idx_offset + own_idx_size, seam_surface.indices(), seam_idx_size);
		}
	}
}

TerrainSynchronizer::ChunkRenderInfo TerrainSynchronizer::slotToRenderInfo(const ChunkSlotSyncData &slot) const noexcept
{
	return {
		// TODO (Svenny): obtaining handles this way is not particularly efficient
		.vertex_buffer = arenaHandle(m_vertex_arenas, slot.vertex_arena_id),
		.index_buffer = arenaHandle(m_index_arenas, slot.index_arena_id),
		.index_type = slot.index_type,
		.first_vertex = int32_t(slot.vertex_arena_offset / sizeof(terrain::SurfaceVertex)),
		.first_index = uint32_t(slot.index_arena_offset / sizeof(uint32_t)),
		.num_vertices = slot.num_vertices,
		.num_indices = slot.num_indices
	};
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

	if (m_vertex_arenas.size() == 1) {
		m_vertex_uma = m_vertex_arenas.back().tryHostMap();
		if (m_vertex_uma) {
			Log::info("Vertex arenas are directly uploadable from host");
		}
	} else if (m_vertex_uma) {
		bool res = m_vertex_arenas.back().tryHostMap();
		if (!res) {
			m_vertex_arenas.pop_back();
			Log::error("Inconsistent vertex arena host visibility!");
			throw MessageException("inconsistent host visibility for same objects");
		}
	}

	Log::debug("Added new vertex arena, {} total now", m_vertex_arenas.size());
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

	if (m_index_arenas.size() == 1) {
		// This is the first index arena added, check if we have host access to it
		m_index_uma = m_index_arenas.back().tryHostMap();
		if (m_index_uma) {
			Log::info("Index arenas are directly uploadable from host");
		}
	} else if (m_index_uma) {
		bool res = m_index_arenas.back().tryHostMap();
		if (!res) {
			m_index_arenas.pop_back();
			Log::info("Inconsistent index arena host visibility!");
			throw MessageException("inconsistent host visibility for same objects");
		}
	}

	Log::debug("Added new index arena, {} total now", m_index_arenas.size());
}

TerrainDataArena &TerrainSynchronizer::selectArena(std::list<TerrainDataArena> &list, uint32_t id) noexcept
{
	assert(id != UINT16_MAX);

	auto iter = list.begin();
	for (uint32_t i = 0; i < id; i++) {
		++iter;
	}
	return *iter;
}

VkBuffer TerrainSynchronizer::arenaHandle(const std::list<TerrainDataArena> &list, uint32_t id) noexcept
{
	if (id == UINT16_MAX) {
		return VK_NULL_HANDLE;
	}

	auto iter = list.begin();
	for (uint32_t i = 0; i < id; i++) {
		++iter;
	}
	return iter->bufferHandle();
}

}
