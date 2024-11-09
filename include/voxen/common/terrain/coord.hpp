#pragma once

#include <voxen/land/chunk_key.hpp>

#include <glm/vec3.hpp>

namespace voxen::terrain
{

// Helper functions to convert between various coordinate systems
class CoordUtils final {
public:
	// Static class, no instances of it are allowed
	CoordUtils() = delete;

	// Convert world-space point to chunk-local [0..CHUNK_SIZE) for a given chunk
	static glm::dvec3 worldToChunkLocal(land::ChunkKey id, const glm::dvec3 &world) noexcept;
	// Convert chunk-local [0..CHUNK_SIZE) point to world-space for a given chunk
	static glm::dvec3 chunkLocalToWorld(land::ChunkKey id, const glm::dvec3 &local) noexcept;
};

} // namespace voxen::terrain
