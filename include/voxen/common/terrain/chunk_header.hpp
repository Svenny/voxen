#pragma once

#include <glm/glm.hpp>

namespace voxen
{

struct TerrainChunkHeader {
	int64_t base_x;
	int64_t base_y;
	int64_t base_z;
	uint32_t scale;

	bool operator == (const TerrainChunkHeader &other) const noexcept;
	uint64_t hash() const noexcept;

	glm::dvec3 worldToLocal(double x, double y, double z) const noexcept;
	glm::dvec3 localToWorld(double x, double y, double z) const noexcept;
};

}
