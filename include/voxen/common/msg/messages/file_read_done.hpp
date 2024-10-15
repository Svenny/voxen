#pragma once

#include <extras/dyn_array.hpp>

#include <cstddef>
#include <cstdint>

namespace voxen::msg
{

struct FileReadDoneMessage {
	static const uint32_t ID;

	uint64_t request_id;
	extras::dyn_array<std::byte> content;
};

} // namespace voxen::msg
