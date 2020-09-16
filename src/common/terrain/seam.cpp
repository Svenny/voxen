#include <voxen/common/terrain/seam.hpp>

#include <voxen/util/hash.hpp>

namespace voxen
{

template<typename T>
TerrainSurface contour(const T &chunks);

template<>
TerrainSurface contour(const TerrainSeam::ChunkPair &pair)
{
	(void) pair;
	return {};
}

template<>
TerrainSurface contour(const TerrainSeam::ChunkQuad &quad)
{
	(void) quad;
	return {};
}

template<>
TerrainSurface contour(const TerrainSeam::ChunkOcto &octo)
{
	(void) octo;
	return {};
}

TerrainSeam::TerrainSeam(const ChunkPair &pair)
	: m_chunks(pair), m_surface(contour(pair))
{
}

TerrainSeam::TerrainSeam(const ChunkQuad &quad)
	: m_chunks(quad), m_surface(contour(quad))
{
}

TerrainSeam::TerrainSeam(const ChunkOcto &octo)
	: m_chunks(octo), m_surface(contour(octo))
{
}

TerrainSeam::TerrainSeam(TerrainSeam &&other) noexcept
	: m_chunks(std::move(other.m_chunks)), m_surface(std::move(other.m_surface))
{
}

TerrainSeam::TerrainSeam(const TerrainSeam &other)
	: m_chunks(other.m_chunks), m_surface(other.m_surface)
{
}

TerrainSeam &TerrainSeam::operator = (TerrainSeam &&other) noexcept
{
	m_chunks = std::move(other.m_chunks);
	m_surface = std::move(other.m_surface);
	return *this;
}

TerrainSeam &TerrainSeam::operator = (const TerrainSeam &other)
{
	m_chunks = other.m_chunks;
	m_surface = other.m_surface;
	return *this;
}

uint64_t TerrainSeam::chunkPointersHash() const noexcept
{
	return std::visit([](auto &&item) {
		return hashFnv1a(item.chunks.data(), sizeof(item.chunks[0]) * item.chunks.size());
	}, m_chunks);
}

}
