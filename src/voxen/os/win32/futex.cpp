#include <voxen/os/futex.hpp>

#include <windows.h>
// --- dont reorder
#include <synchapi.h>

#include <cassert>

namespace voxen::os
{

void Futex::waitInfinite(std::atomic_uint32_t *addr, uint32_t value) noexcept
{
	[[maybe_unused]] BOOL res = WaitOnAddress(addr, &value, 4, INFINITE);
	// Nothing should fail here
	assert(res == TRUE);
}

void Futex::wakeSingle(std::atomic_uint32_t *addr) noexcept
{
	WakeByAddressSingle(addr);
}

} // namespace voxen::os
