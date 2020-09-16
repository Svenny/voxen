#pragma once

#include <voxen/common/terrain/chunk.hpp>
#include <voxen/common/terrain/surface.hpp>

#include <array>
#include <variant>

namespace voxen
{

enum class Axis {
	X,
	Y,
	Z
};

class TerrainSeam {
public:
	struct ChunkPair {
		std::array<const TerrainChunk *, 2> chunks;
		Axis axis;
	};

	struct ChunkQuad {
		std::array<const TerrainChunk *, 4> chunks;
		Axis axis;
	};

	struct ChunkOcto {
		std::array<const TerrainChunk *, 8> chunks;
	};

	explicit TerrainSeam(const ChunkPair &pair);
	explicit TerrainSeam(const ChunkQuad &quad);
	explicit TerrainSeam(const ChunkOcto &octo);
	TerrainSeam(TerrainSeam &&) noexcept;
	TerrainSeam(const TerrainSeam &);
	TerrainSeam &operator = (TerrainSeam &&) noexcept;
	TerrainSeam &operator = (const TerrainSeam &);
	~TerrainSeam() = default;

	uint64_t chunkPointersHash() const noexcept;

	TerrainSurface &surface() noexcept { return m_surface; }
	const TerrainSurface &surface() const noexcept { return m_surface; }
private:
	std::variant<ChunkPair, ChunkQuad, ChunkOcto> m_chunks;
	TerrainSurface m_surface;
};

}
