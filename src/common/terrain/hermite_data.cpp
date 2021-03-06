#include <voxen/common/terrain/hermite_data.hpp>

#include <algorithm>

namespace voxen
{

static uint32_t doubleTo24Unorm(double value) noexcept
{
	return uint32_t(value * 16777215.0);
}

static double unorm24ToDouble(uint32_t value) noexcept
{
	return double(value) / 16777215.0;
}

// -- HermiteDataEntry ---

HermiteDataEntry::HermiteDataEntry(coord_t lesser_x, coord_t lesser_y, coord_t lesser_z,
                                   const glm::vec3 &normal, double offset,
                                   Axis axis, bool is_lesser_endpoint_solid, voxel_t solid_voxel) noexcept
	: m_axis(uint32_t(axis)), m_solid_voxel(solid_voxel),
	  m_lesser_x(lesser_x), m_lesser_y(lesser_y), m_lesser_z(lesser_z)
{
	m_normal_x = normal.x;
	m_normal_z = normal.z;
	m_offset = doubleTo24Unorm(offset);
	m_normal_y_sign = normal.y < 0.0f ? 1 : 0;
	m_solid_endpoint = is_lesser_endpoint_solid ? 0 : 1;
}

glm::vec3 HermiteDataEntry::surfaceNormal() const noexcept
{
	float y_squared = 1.0f - m_normal_x * m_normal_x - m_normal_z * m_normal_z;
	float normal_y = glm::sqrt(glm::max(0.0f, y_squared));
	if (m_normal_y_sign)
		normal_y = -normal_y;
	return { m_normal_x, normal_y, m_normal_z };
}

glm::vec3 HermiteDataEntry::surfacePoint() const noexcept
{
	glm::vec3 point(m_lesser_x, m_lesser_y, m_lesser_z);
	point[m_axis] += unorm24ToDouble(m_offset);
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

bool HermiteDataEntry::operator == (const HermiteDataEntry &other) const noexcept
{
	return m_normal_x == other.m_normal_x && m_normal_z == other.m_normal_z &&
		m_offset == other.m_offset &&
		m_normal_y_sign == other.m_normal_y_sign &&
		m_solid_endpoint == other.m_solid_endpoint &&
		m_axis == other.m_axis &&
		m_solid_voxel == other.m_solid_voxel &&
		m_lesser_x == other.m_lesser_x && m_lesser_y == other.m_lesser_y && m_lesser_z == other.m_lesser_z;
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
