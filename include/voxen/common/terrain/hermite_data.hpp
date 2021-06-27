#pragma once

#include <voxen/common/terrain/config.hpp>
#include <voxen/util/allocator.hpp>

#include <glm/vec3.hpp>

#include <vector>

namespace voxen::terrain
{

class HermiteDataEntry {
public:
	/// Type alias for local coordinates
	using coord_t = uint8_t;

	HermiteDataEntry() = default;
	/** \brief Main constructor

		\param[in] lesser_x,lesser_y,lesser_z Local coordinates of the lesser endpoint
		\param[in] normal Surface normal in surface-crossing point
		\param[in] offset Surface-crossing point offset from lesser endpoint (in local coordinates)
		\param[in] axis Axis of this edge in GLM order (X=0, Y=1, Z=2)
		\param[in] is_lesser_endpoint_solid Self-descriptive
		\param[in] solid_voxel Voxel ID of the solid endpoint
	*/
	explicit HermiteDataEntry(coord_t lesser_x, coord_t lesser_y, coord_t lesser_z,
	                          const glm::vec3 &normal, double offset,
	                          int axis, bool is_lesser_endpoint_solid, voxel_t solid_voxel) noexcept;
	HermiteDataEntry(HermiteDataEntry &&) = default;
	HermiteDataEntry(const HermiteDataEntry &) = default;
	HermiteDataEntry &operator = (HermiteDataEntry &&) = default;
	HermiteDataEntry &operator = (const HermiteDataEntry &) = default;
	~HermiteDataEntry() = default;

	/// Returns surface normal in the surface-crossing point on this edge
	glm::vec3 surfaceNormal() const noexcept;
	/// Returns local coordinates of the surface-crossing point on this edge
	glm::vec3 surfacePoint() const noexcept;
	/// Returns the material of the solid endpoint
	voxel_t solidEndpointVoxel() const noexcept { return m_solid_voxel; }
	/// Returns local coordinates of the lesser endpoint
	glm::ivec3 lesserEndpoint() const noexcept;
	/// Returns local coordinates of the bigger endpoint
	glm::ivec3 biggerEndpoint() const noexcept;
	/// Returns true if the lesser endpoint is solid, false otherwise
	bool isLesserEndpointSolid() const noexcept { return m_solid_endpoint == 0; }

private:
	/** \brief Surface normal in zero-crossing point

		Only X and Z components (and sign of Y) are stored because absolute value of Y
		may be restored using the unit length condition. This saves 4 bytes of size.
	*/
	float m_normal_x, m_normal_z;
	/// Offset from lesser endpoint (in local coordinates) expressed as normalized 24-bit value
	uint32_t m_offset : 24;
	/// 0 if Y is positive, 1 if negative
	uint32_t m_normal_y_sign : 1;
	/// 0 if solid endpoint is the lesser one, 1 otherwise
	uint32_t m_solid_endpoint : 1;
	/// Edge axis in GLM order
	uint32_t m_axis : 2;
	/// Material of solid endpoint
	voxel_t m_solid_voxel;
	/// Local coordinates of the lesser endpoint
	coord_t m_lesser_x, m_lesser_y, m_lesser_z;

	friend class HermiteDataStorage;
};
static_assert(sizeof(HermiteDataEntry) == 16, "16-byte Hermite data packing is broken");

/** \brief Compressed storage of `HermiteDataEntry`s for a single axis
*/
class HermiteDataStorage {
public:
	using AllocatorType = DomainAllocator<HermiteDataEntry, AllocationDomain::TerrainHermite>;
	using StorageType = std::vector<HermiteDataEntry, AllocatorType>;

	using iterator = StorageType::iterator;
	using const_iterator = StorageType::const_iterator;
	using size_type = StorageType::size_type;
	using coord_t = HermiteDataEntry::coord_t;

	template<typename... Args>
	void emplace(Args &&... args) { m_storage.emplace_back(std::forward<Args>(args)...); }
	/** \brief Sorts stored edges by lesser endpoints (in YXZ order)

		Using \ref findEdge is possible only when edges are sorted. Instead of calling this
		function you may enforce adding entries in given order (if possible).
	*/
	void sort() noexcept;
	void clear() noexcept { m_storage.clear(); }
	iterator begin() noexcept { return m_storage.begin(); }
	iterator end() noexcept { return m_storage.end(); }
	/** \brief Finds an entry with given lesser endpoint coordinates

		\param[in] x,y,z Local coordinates of the lesser endpoint
		\return Iterator to the found entry or \ref end in case nothing was found
		\attention This function runs binary search. Make sure storage is sorted before calling
	*/
	iterator find(coord_t x, coord_t y, coord_t z) noexcept;
	/// \copydoc find
	const_iterator find(coord_t x, coord_t y, coord_t z) const noexcept;
	/// Returns the number of currently stored entries
	size_type size() const noexcept { return m_storage.size(); }
	const_iterator begin() const noexcept { return m_storage.begin(); }
	const_iterator end() const noexcept { return m_storage.end(); }
	const_iterator cbegin() const noexcept { return m_storage.cbegin(); }
	const_iterator cend() const noexcept { return m_storage.cend(); }

private:
	/// Wrapped container
	StorageType m_storage;
	/// 'Less' comparator for entries, orders them as (Y, X, Z) tuples
	static bool entryLess(const HermiteDataEntry &a, const HermiteDataEntry &b) noexcept;
};

}
