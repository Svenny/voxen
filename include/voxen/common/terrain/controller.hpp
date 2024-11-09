#pragma once

#include <voxen/common/terrain/control_block.hpp>
#include <voxen/common/terrain/loader.hpp>

#include <extras/refcnt_ptr.hpp>

#include <glm/vec3.hpp>

#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace voxen
{

class ThreadPool;

}

namespace voxen::svc
{

class ServiceLocator;

}

namespace voxen::terrain
{

class Chunk;
class ChunkControlBlock;

class Controller final {
public:
	using ChunkPtr = extras::refcnt_ptr<Chunk>;
	using ControlBlockPtr = std::unique_ptr<ChunkControlBlock>;

	Controller(svc::ServiceLocator &svc);
	Controller(Controller &&) = delete;
	Controller(const Controller &&) = delete;
	Controller &operator=(Controller &&) = delete;
	Controller &operator=(const Controller &) = delete;
	~Controller();

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

	ThreadPool &m_thread_pool;
	TerrainLoader m_loader;
	std::vector<PointOfInterest> m_points_of_interest;
	std::unordered_map<glm::ivec3, SuperchunkInfo, VecHasher> m_superchunks;

	std::unordered_map<land::ChunkKey, std::future<void>> m_async_chunk_loads;
	uint32_t m_direct_op_quota = 0;

	uint32_t calcLodDirection(land::ChunkKey id) const;
	void garbageCollectPointsOfInterest();
	void engageSuperchunks();

	ControlBlockPtr loadSuperchunk(glm::ivec3 base);
	ControlBlockPtr enqueueLoadingChunk(land::ChunkKey id);

	bool updateChunk(ChunkControlBlock &cb, ParentCommand parent_cmd = ParentCommand::Nothing);
	InnerUpdateResult updateChunkLoading(ChunkControlBlock &cb, ParentCommand parent_cmd);
	InnerUpdateResult updateChunkStandby(ChunkControlBlock &cb, ParentCommand parent_cmd);
	InnerUpdateResult updateChunkActive(ChunkControlBlock &cb, ParentCommand parent_cmd);
};

} // namespace voxen::terrain
