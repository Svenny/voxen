#pragma once

#include <voxen/util/packed_color.hpp>

#include <string_view>

namespace voxen::land
{

class IBlock {
public:
	virtual ~IBlock() noexcept;

	virtual std::string_view getInternalName() const noexcept = 0;
	virtual PackedColorLinear getImpostorColor() const noexcept = 0;
};

} // namespace voxen::land
