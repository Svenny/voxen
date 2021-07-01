#include <voxen/common/terrain/coord.hpp>

#include <voxen/common/terrain/config.hpp>

#include <cassert>

namespace voxen::terrain
{

glm::dvec3 CoordUtils::worldToChunkLocal(ChunkId id, const glm::dvec3 &world) noexcept
{
	glm::ivec3 base(id.base_x, id.base_y, id.base_z);
	base *= Config::CHUNK_SIZE;
	base >>= id.lod;
	return world / double(1u << id.lod) - glm::dvec3(base);
}

}
