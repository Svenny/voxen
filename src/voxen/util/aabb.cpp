#include <voxen/util/aabb.hpp>

#include <glm/common.hpp>
#include <glm/vector_relational.hpp>

#include <cassert>
#include <cfloat>

namespace voxen
{

Aabb::Aabb() noexcept : m_min(FLT_MAX, FLT_MAX, FLT_MAX), m_max(FLT_MIN, FLT_MIN, FLT_MIN) {}

void Aabb::mergeWith(const Aabb &other) noexcept
{
	m_min = glm::min(m_min, other.m_min);
	m_max = glm::max(m_max, other.m_max);
}

void Aabb::includePoint(const glm::vec3 &point) noexcept
{
	m_min = glm::min(m_min, point);
	m_max = glm::max(m_max, point);
}

bool Aabb::isPointInside(const glm::vec3 &point) const noexcept
{
	return glm::clamp(point, m_min, m_max) == point;
}

bool Aabb::isValid() const noexcept
{
	return glm::lessThanEqual(m_min, m_max) == glm::bvec3(true);
}

} // namespace voxen
