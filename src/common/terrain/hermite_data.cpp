#include <voxen/common/terrain/hermite_data.hpp>

#include <cassert>
#include <cmath>
#include <algorithm>

namespace voxen::terrain
{

static uint32_t doubleTo24Unorm(double value) noexcept
{
	return uint32_t(value * 16777215.0);
}

static float unorm24ToFloat(uint32_t value) noexcept
{
	return float(value) / 16777215.0f;
}

// --- HermiteDataEntry ---

HermiteDataEntry::HermiteDataEntry(coord_t lesser_x, coord_t lesser_y, coord_t lesser_z,
                                   const glm::vec3 &normal, double offset,
                                   int axis, bool is_lesser_endpoint_solid, voxel_t solid_voxel) noexcept
	: m_axis(uint32_t(axis)), m_solid_voxel(solid_voxel),
	  m_lesser_x(lesser_x), m_lesser_y(lesser_y), m_lesser_z(lesser_z)
{
	assert(offset >= 0.0 && offset <= 1.0);
	assert(axis >= 0 && axis <= 2);
	assert(solid_voxel != 0);

	m_normal_x = normal.x;
	m_normal_z = normal.z;
	m_offset = doubleTo24Unorm(offset);
	m_normal_y_sign = normal.y < 0.0f ? 1 : 0;
	m_solid_endpoint = is_lesser_endpoint_solid ? 0 : 1;
}

glm::vec3 HermiteDataEntry::surfaceNormal() const noexcept
{
	float y_squared = 1.0f - m_normal_x * m_normal_x - m_normal_z * m_normal_z;
	float normal_y = std::sqrt(std::max(0.0f, y_squared));
	if (m_normal_y_sign)
		normal_y = -normal_y;
	return { m_normal_x, normal_y, m_normal_z };
}

glm::vec3 HermiteDataEntry::surfacePoint() const noexcept
{
	glm::vec3 point(m_lesser_x, m_lesser_y, m_lesser_z);
	point[m_axis] += unorm24ToFloat(m_offset);
	return point;
}

glm::ivec3 HermiteDataEntry::lesserEndpoint() const noexcept
{
	return { m_lesser_x, m_lesser_y, m_lesser_z };
}

glm::ivec3 HermiteDataEntry::biggerEndpoint() const noexcept
{
	glm::ivec3 point(m_lesser_x, m_lesser_y, m_lesser_z);
	point[m_axis]++;
	return point;
}

// --- HermiteDataStorage ---

bool HermiteDataStorage::entryLess(const HermiteDataEntry &a, const HermiteDataEntry &b) noexcept
{
	// Compare as (Y, X, Z) tuples
	return std::make_tuple(a.m_lesser_y, a.m_lesser_x, a.m_lesser_z) <
	       std::make_tuple(b.m_lesser_y, b.m_lesser_x, b.m_lesser_z);
}

void HermiteDataStorage::sort() noexcept
{
	std::sort(m_storage.begin(), m_storage.end(), entryLess);
}

HermiteDataStorage::iterator HermiteDataStorage::find(coord_t x, coord_t y, coord_t z) noexcept
{
	HermiteDataEntry sample;
	sample.m_lesser_x = x;
	sample.m_lesser_y = y;
	sample.m_lesser_z = z;
	auto iter = std::lower_bound(m_storage.begin(), m_storage.end(), sample, entryLess);
	if (iter == m_storage.end())
		return iter;
	if (iter->m_lesser_x != x || iter->m_lesser_y != y || iter->m_lesser_z != z) {
		// `lower_bound` found wrong value, this means there is no `sample` in the container
		return end();
	}
	return iter;
}

HermiteDataStorage::const_iterator HermiteDataStorage::find(coord_t x, coord_t y, coord_t z) const noexcept
{
	HermiteDataEntry sample;
	sample.m_lesser_x = x;
	sample.m_lesser_y = y;
	sample.m_lesser_z = z;
	auto iter = std::lower_bound(m_storage.begin(), m_storage.end(), sample, entryLess);
	if (iter == m_storage.end()) {
		return iter;
	}
	if (iter->m_lesser_x != x || iter->m_lesser_y != y || iter->m_lesser_z != z) {
		// `lower_bound` found wrong value, this means there is no `sample` in the container
		return end();
	}
	return iter;
}

}
