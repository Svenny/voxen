#pragma once

#include <voxen/common/terrain/surface_builder.hpp>

#include <extras/refcnt_ptr.hpp>

#include <memory>

namespace voxen::terrain
{

class Chunk;

class ChunkControlBlock final {
public:
	enum class State {
		Invalid,
		Loading,
		Standby,
		Active,
	};

	ChunkControlBlock() = default;
	ChunkControlBlock(ChunkControlBlock &&) = delete;
	ChunkControlBlock(const ChunkControlBlock &) = delete;
	ChunkControlBlock &operator=(ChunkControlBlock &&) = delete;
	ChunkControlBlock &operator=(const ChunkControlBlock &) = delete;
	~ChunkControlBlock() = default;

	void setState(State state) noexcept { m_state = state; }
	void setOverActive(bool value) noexcept { m_over_active = value; }
	void setChunkChanged(bool value) noexcept { m_chunk_changed = value; }

	void setChunk(extras::refcnt_ptr<Chunk> ptr);
	void setChild(size_t id, std::unique_ptr<ChunkControlBlock> ptr) noexcept { m_children[id] = std::move(ptr); }

	// Go DFS over this chunk and its children and assert some invariants about their state.
	// NOTE: this function consists only of asserts so it has no effect in release builds.
	void validateState(bool has_active_parent = false, bool can_chunk_changed = true) const;

	State state() const noexcept { return m_state; }
	bool isOverActive() const noexcept { return m_over_active; }
	bool isChunkChanged() const noexcept { return m_chunk_changed; }

	extras::refcnt_ptr<Chunk> chunkPtr() const noexcept { return m_chunk; }

	ChunkControlBlock *child(size_t id) noexcept { return m_children[id].get(); }
	Chunk *chunk() noexcept { return m_chunk.get(); }

	const ChunkControlBlock *child(size_t id) const noexcept { return m_children[id].get(); }
	const Chunk *chunk() const noexcept { return m_chunk.get(); }

private:
	State m_state = State::Invalid;
	bool m_over_active = false;
	bool m_chunk_changed = false;

	std::unique_ptr<ChunkControlBlock> m_children[8];

	extras::refcnt_ptr<Chunk> m_chunk;
};

} // namespace voxen::terrain
