#pragma once

#include <voxen/common/terrain/loader.hpp>
#include <voxen/common/terrain/control_block.hpp>

#include <extras/refcnt_ptr.hpp>

#include <glm/vec3.hpp>

#include <cstdint>
#include <future>
#include <memory>
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
	using ChunkPtr = extras::refcnt_ptr<Chunk>;
	using ControlBlockPtr = std::unique_ptr<ChunkControlBlock>;

	Controller() = default;
	Controller(Controller &&) = delete;
	Controller(const Controller &&) = delete;
	Controller &operator = (Controller &&) = delete;
	Controller &operator = (const Controller &) = delete;
	~Controller() = default;

	std::vector<ChunkPtr> doTick();
	void setPointOfInterest(uint32_t id, const glm::dvec3 &position);

private:
	enum class ParentCommand {
		Nothing,
		BecomeActive,
		BecomeStandby,
		Unload
	};

	using InnerUpdateResult = std::pair<bool, ParentCommand>;
	using OuterUpdateResult = std::optional<ControlBlockPtr>;

	struct PointOfInterest {
		uint32_t id;
		uint32_t age;
		glm::dvec3 position;
	};

	struct SuperchunkInfo {
		ControlBlockPtr ptr;
		uint32_t idle_age;
	};

	struct VecHasher {
		size_t operator()(const glm::ivec3 &v) const noexcept;
	};

	TerrainLoader m_loader;
	std::vector<PointOfInterest> m_points_of_interest;
	std::unordered_map<glm::ivec3, SuperchunkInfo, VecHasher> m_superchunks;

	std::unordered_map<ChunkId, std::future<extras::refcnt_ptr<Chunk>>> m_async_chunk_loads;
	uint32_t m_load_quota = 0;

	uint32_t calcLodDirection(ChunkId id) const;
	void garbageCollectPointsOfInterest();
	void engageSuperchunks();

	ControlBlockPtr loadSuperchunk(glm::ivec3 base);
	ControlBlockPtr enqueueLoadingChunk(ChunkId id);

	bool updateChunk(ChunkControlBlock &cb, ParentCommand parent_cmd = ParentCommand::Nothing);
	InnerUpdateResult updateChunkLoading(ChunkControlBlock &cb, ParentCommand parent_cmd);
	InnerUpdateResult updateChunkStandby(ChunkControlBlock &cb, ParentCommand parent_cmd);
	InnerUpdateResult updateChunkActive(ChunkControlBlock &cb, ParentCommand parent_cmd);

	// Seam-related fucntions are implemented in `controller_seam_ops.cpp` due to their sheer size

	template<int D>
	void seamEdgeProcPhase1(std::array<ChunkControlBlock *, 4> nodes);
	template<int D>
	void seamFaceProcPhase1(std::array<ChunkControlBlock *, 2> nodes);
	void seamCellProcPhase1(ChunkControlBlock *node);

	template<int D>
	void seamEdgeProcPhase2(std::array<ChunkControlBlock *, 4> nodes);
	template<int D>
	void seamFaceProcPhase2(std::array<ChunkControlBlock *, 2> nodes);
	void seamCellProcPhase2(ChunkControlBlock *node);

	void updateCrossSuperchunkSeams();
};

}
