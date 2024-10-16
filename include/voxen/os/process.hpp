#pragma once

#include <cstdint>

namespace voxen::os
{

namespace Process
{

int32_t getProcessId() noexcept;
int32_t getThreadId() noexcept;

} // namespace Process

} // namespace voxen::os
