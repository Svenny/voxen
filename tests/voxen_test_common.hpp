#pragma once

#include "test_common.hpp"

#include <voxen/util/error_condition.hpp>
#include <voxen/util/exception.hpp>

namespace voxen::test
{

// Usage: `CHECK_THROWS_MATCHES(<expr>, Exception, errcExceptionMatcher(VoxenErrc::<code>))`
inline auto errcExceptionMatcher(VoxenErrc ec)
{
	return Catch::Predicate<Exception>([ec](const Exception& ex) { return ex.error() == ec; });
}

} // namespace voxen::test
