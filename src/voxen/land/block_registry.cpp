#include <voxen/land/block_registry.hpp>

#include <voxen/util/error_condition.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include <cassert>

namespace voxen::land
{

BlockRegistry::BlockRegistry()
{
	// Pseudo-registration to use zero ID (none-block)
	m_registered_blocks.emplace_back(nullptr);
	m_impostor_colors.emplace_back(PackedColorLinear::transparentBlack());
}

uint16_t BlockRegistry::registerBlock(std::shared_ptr<IBlock> ptr)
{
	assert(ptr);

	if (m_registered_blocks.size() > UINT16_MAX) [[unlikely]] {
		Log::error("Block ID space exhausted! {} registered blocks, trying to register '{}'", m_registered_blocks.size(),
			ptr->getInternalName());
		throw Exception::fromError(VoxenErrc::OutOfResource, "block ID space exhausted");
	}

	uint16_t id = static_cast<uint16_t>(m_registered_blocks.size());

	m_impostor_colors.emplace_back(ptr->getImpostorColor());
	m_registered_blocks.emplace_back(std::move(ptr));

	return id;
}

} // namespace voxen::land
