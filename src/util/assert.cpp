#include <voxen/util/assert.hpp>
#include <voxen/util/log.hpp>

namespace voxen
{

void vxAssertFail(std::experimental::source_location where) noexcept {
	Log::fatal("Assertion failed!", where);
	// TODO: do something smarter here? Print backtrace?
	abort();
}

}
