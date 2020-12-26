#include <voxen/common/terrain/chunk_header.hpp>

#include <voxen/util/hash.hpp>

namespace voxen
{

bool TerrainChunkHeader::operator== (const TerrainChunkHeader &other) const noexcept {
	return base_x == other.base_x && base_y == other.base_y && base_z == other.base_z && scale == other.scale;
}

uint64_t TerrainChunkHeader::hash() const noexcept
{
#pragma pack(push, 1)
	struct {
		uint64_t u64[3];
		uint32_t u32;
	} data;
#pragma pack(pop)

	data.u64[0] = static_cast<uint64_t>(base_x);
	data.u64[1] = static_cast<uint64_t>(base_y);
	data.u64[2] = static_cast<uint64_t>(base_z);
	data.u32 = (scale);

	return hashFnv1a(&data, sizeof(data));
}

glm::dvec3 TerrainChunkHeader::worldToLocal(double x, double y, double z) const noexcept
{
	glm::dvec3 p(x, y, z);
	p -= glm::dvec3(base_x, base_y, base_z);
	p /= double(scale);
	return p;
}

glm::dvec3 TerrainChunkHeader::localToWorld(double x, double y, double z) const noexcept
{
	glm::dvec3 p(x, y, z);
	p *= double(scale);
	p += glm::dvec3(base_x, base_y, base_z);
	return p;
}

}
