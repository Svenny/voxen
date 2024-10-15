#pragma once

#include <voxen/common/land/block.hpp>

#include <memory>
#include <vector>

namespace voxen::land
{

class BlockRegistry {
public:
	BlockRegistry();

	uint16_t registerBlock(std::shared_ptr<IBlock> ptr);

	PackedColorLinear getImpostorColor(uint16_t id) const noexcept { return m_impostor_colors[id]; }

private:
	std::vector<std::shared_ptr<IBlock>> m_registered_blocks;
	std::vector<PackedColorLinear> m_impostor_colors;
};

} // namespace voxen::land
