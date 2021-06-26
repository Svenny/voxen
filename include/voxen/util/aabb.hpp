#pragma once

#include <glm/vec3.hpp>

#include <cfloat>

namespace voxen
{

// 3D axis-aligned bounding box
class Aabb {
public:
	// Initially AABB is invalid - that is, its `min()` is larger than `max()`.
	// Operations on it will return undefined values until the first call
	// to expanding method, such as `mergeWidth()` or `includePoint()`.
	Aabb() = default;
	Aabb(Aabb &&) = default;
	Aabb(const Aabb &) = default;
	Aabb &operator = (Aabb &&) = default;
	Aabb &operator = (const Aabb &) = default;
	~Aabb() = default;

	// Expand this AABB to also include `other` AABB
	void mergeWith(const Aabb &other) noexcept;
	// Expand this AABB to include `point`
	void includePoint(const glm::vec3 &point) noexcept;
	// Check whether `point` is inside this AABB
	bool isPointInside(const glm::vec3 &point) const noexcept;
	// Check whether this AABB's `min()` is less than or equal to `max()`.
	// `isValid()` will be `false` for newly created AABB.
	bool isValid() const noexcept;

	const glm::vec3 &min() const noexcept { return m_min; }
	const glm::vec3 &max() const noexcept { return m_max; }

private:
	glm::vec3 m_min { FLT_MAX, FLT_MAX, FLT_MAX };
	glm::vec3 m_max { FLT_MIN, FLT_MIN, FLT_MIN };
};

}
