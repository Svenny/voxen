#pragma once

#include <voxen/common/terrain/surface_builder.hpp>

#include <extras/refcnt_ptr.hpp>

#include <memory>

namespace voxen::terrain
{

class Chunk;

class ChunkControlBlock final {
public:
	struct CreationInfo final {
		const ChunkControlBlock *predecessor;
		bool reset_seam;
	};

	enum class State {
		Invalid,
		Loading,
		Standby,
		Active,
	};

	explicit ChunkControlBlock(CreationInfo info);
	ChunkControlBlock() = delete;
	ChunkControlBlock(ChunkControlBlock &&) = delete;
	ChunkControlBlock(const ChunkControlBlock &) = delete;
	ChunkControlBlock &operator = (ChunkControlBlock &&) = delete;
	ChunkControlBlock &operator = (const ChunkControlBlock &) = delete;
	~ChunkControlBlock() noexcept;

	void setState(State state) noexcept { m_state = state; }
	void setSeamDirty(bool value) noexcept { m_seam_dirty = value; }
	void setOverActive(bool value) noexcept { m_over_active = value; }

	void setChunk(extras::refcnt_ptr<Chunk> ptr);
	void setChild(unsigned id, extras::refcnt_ptr<ChunkControlBlock> ptr) noexcept { m_children[id] = std::move(ptr); }

	// Go DFS over this chunk and its children and assert some invariants about their state.
	// NOTE: this function consists only of asserts so it has no effect in release builds.
	void validateState(bool has_active_parent = false, bool can_seam_dirty = true) const;
	// Go DFS over this chunk and its children, collect and log some statistics.
	// NOTE: this function is intended for debug use only and does nothing in release builds.
	void printStats() const;

	State state() const noexcept { return m_state; }
	bool isSeamDirty() const noexcept { return m_seam_dirty; }
	bool isOverActive() const noexcept { return m_over_active; }

	extras::refcnt_ptr<Chunk> chunkPtr() const noexcept { return m_chunk; }

	SurfaceBuilder &surfaceBuilder() noexcept { return *m_surface_builder; }
	Chunk *chunk() noexcept { return m_chunk.get(); }
	ChunkControlBlock *child(int id) noexcept { return m_children[id].get(); }

	const SurfaceBuilder &surfaceBuilder() const noexcept { return *m_surface_builder; }
	const Chunk *chunk() const noexcept { return m_chunk.get(); }
	const ChunkControlBlock *child(int id) const noexcept { return m_children[id].get(); }

private:
	State m_state = State::Invalid;
	bool m_seam_dirty = false;
	bool m_over_active = false;
	std::unique_ptr<SurfaceBuilder> m_surface_builder;

	extras::refcnt_ptr<Chunk> m_chunk;
	extras::refcnt_ptr<ChunkControlBlock> m_children[8];
};

}
