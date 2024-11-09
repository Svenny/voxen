#include <voxen/common/terrain/coord.hpp>

#include <voxen/common/terrain/config.hpp>

#include <cassert>

namespace voxen::terrain
{

glm::dvec3 CoordUtils::worldToChunkLocal(land::ChunkKey id, const glm::dvec3 &world) noexcept
{
	glm::ivec3 base(id.x, id.y, id.z);
	base *= Config::CHUNK_SIZE;
	base >>= id.scale_log2;
	return world / double(1u << id.scale_log2) - glm::dvec3(base);
}

glm::dvec3 CoordUtils::chunkLocalToWorld(land::ChunkKey id, const glm::dvec3 &local) noexcept
{
	glm::ivec3 base(id.x, id.y, id.z);
	base *= Config::CHUNK_SIZE;
	return local * double(1u << id.scale_log2) + glm::dvec3(base);
}

} // namespace voxen::terrain
