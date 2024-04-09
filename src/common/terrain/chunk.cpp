#include <voxen/common/terrain/chunk.hpp>

#include <cassert>

namespace voxen::terrain
{

Chunk::Chunk(CreationInfo info) : m_id(info.id), m_version(info.version) {}

Chunk &Chunk::operator=(Chunk &&other) noexcept
{
	assert(m_id == other.m_id);

	m_version = other.m_version;

	std::swap(m_primary_data, other.m_primary_data);
	std::swap(m_surface, other.m_surface);

	return *this;
}

bool Chunk::hasSurface() const noexcept
{
	return !m_primary_data.hermite_data_x.empty() || !m_primary_data.hermite_data_y.empty()
		|| !m_primary_data.hermite_data_z.empty();
}

} // namespace voxen::terrain
