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
	ChunkControlBlock &operator = (ChunkControlBlock &&) = delete;
	ChunkControlBlock &operator = (const ChunkControlBlock &) = delete;
	~ChunkControlBlock() = default;

	void setState(State state) noexcept { m_state = state; }
	void setOverActive(bool value) noexcept { m_over_active = value; }
	void setChunkChanged(bool value) noexcept { m_chunk_changed = value; }
	void setInducedSeamDirty(bool value) noexcept { m_induced_seam_dirty = value; }

	void clearTemporaryFlags() noexcept;

	void copyChunk();
	void setChunk(extras::refcnt_ptr<Chunk> ptr);
	void setChild(int id, std::unique_ptr<ChunkControlBlock> ptr) noexcept { m_children[id] = std::move(ptr); }

	// Go DFS over this chunk and its children and assert some invariants about their state.
	// NOTE: this function consists only of asserts so it has no effect in release builds.
	void validateState(bool has_active_parent = false, bool can_seam_dirty = true, bool can_chunk_changed = true) const;
	// Go DFS over this chunk and its children, collect and log some statistics.
	// NOTE: this function is intended for debug use only and does nothing in release builds.
	void printStats() const;

	State state() const noexcept { return m_state; }
	bool isOverActive() const noexcept { return m_over_active; }
	bool isChunkCopied() const noexcept { return m_chunk_copied; }
	bool isChunkChanged() const noexcept { return m_chunk_changed; }
	bool isInducedSeamDirty() const noexcept { return m_induced_seam_dirty; }

	extras::refcnt_ptr<Chunk> chunkPtr() const noexcept { return m_chunk; }

	ChunkControlBlock *child(int id) noexcept { return m_children[id].get(); }
	Chunk *chunk() noexcept { return m_chunk.get(); }
	SurfaceBuilder &surfaceBuilder() noexcept { return m_surface_builder; }

	const ChunkControlBlock *child(int id) const noexcept { return m_children[id].get(); }
	const Chunk *chunk() const noexcept { return m_chunk.get(); }
	const SurfaceBuilder &surfaceBuilder() const noexcept { return m_surface_builder; }

private:
	State m_state = State::Invalid;
	bool m_over_active = false;
	bool m_chunk_copied = false;
	bool m_chunk_changed = false;
	bool m_induced_seam_dirty = false;

	std::unique_ptr<ChunkControlBlock> m_children[8];

	extras::refcnt_ptr<Chunk> m_chunk;
	SurfaceBuilder m_surface_builder;
};

}
