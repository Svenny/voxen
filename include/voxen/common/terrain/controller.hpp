#pragma once

#include <voxen/common/terrain/loader.hpp>

#include <extras/refcnt_ptr.hpp>

#include <glm/vec3.hpp>

#include <cstdint>
#include <future>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace voxen::terrain
{

class Chunk;
class ChunkControlBlock;

class Controller final {
public:
	using ControlBlockPtr = extras::refcnt_ptr<ChunkControlBlock>;

	Controller();
	Controller(Controller &&) = delete;
	Controller(const Controller &&) = delete;
	Controller &operator = (Controller &&) = delete;
	Controller &operator = (const Controller &) = delete;
	~Controller() noexcept;

	ControlBlockPtr doTick();
	void setPointOfInterest(uint64_t id, const glm::dvec3 &position);

private:
	enum class ParentCommand {
		Nothing,
		BecomeActive,
		BecomeStandby
	};

	using InnerUpdateResult = std::pair<std::optional<ControlBlockPtr>, ParentCommand>;
	using OuterUpdateResult = std::optional<ControlBlockPtr>;

	struct PointOfInterest {
		uint64_t id;
		uint64_t last_update_tick_id;
		glm::dvec3 position;
	};

	uint64_t m_tick_id = 0;
	TerrainLoader m_loader;
	extras::refcnt_ptr<ChunkControlBlock> m_root_cb;
	std::vector<PointOfInterest> m_points_of_interest;

	std::unordered_set<ChunkControlBlock *> m_new_cbs;
	std::unordered_map<ChunkId, std::future<extras::refcnt_ptr<Chunk>>> m_async_chunk_loads;
	uint32_t m_load_quota = 0;

	uint32_t calcLodDirection(ChunkId id) const;
	void garbageCollectPointsOfInterest();
	ControlBlockPtr copyOnWrite(const ChunkControlBlock &cb, bool mark_seam_dirty = false);
	ControlBlockPtr enqueueLoadingChunk(ChunkId id);

	OuterUpdateResult updateChunk(const ChunkControlBlock &cb, ParentCommand parent_cmd = ParentCommand::Nothing);
	InnerUpdateResult updateChunkLoading(const ChunkControlBlock &cb, ParentCommand parent_cmd);
	InnerUpdateResult updateChunkStandby(const ChunkControlBlock &cb, ParentCommand parent_cmd);
	InnerUpdateResult updateChunkActive(const ChunkControlBlock &cb, ParentCommand parent_cmd);

	// Seam-related fucntions are implemented in `controller_seam_ops.cpp` due to their sheer size

	template<int D>
	std::array<OuterUpdateResult, 4> seamEdgeProcPhase1(std::array<ChunkControlBlock *, 4> nodes);
	template<int D>
	std::array<OuterUpdateResult, 2> seamFaceProcPhase1(std::array<ChunkControlBlock *, 2> nodes);
	OuterUpdateResult seamCellProcPhase1(ChunkControlBlock *node);

	template<int D>
	void seamEdgeProcPhase2(std::array<ChunkControlBlock *, 4> nodes);
	template<int D>
	void seamFaceProcPhase2(std::array<ChunkControlBlock *, 2> nodes);
	void seamCellProcPhase2(ChunkControlBlock *node);
};

}
