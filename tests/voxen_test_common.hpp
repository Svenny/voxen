#pragma once

#include "test_common.hpp" // IWYU pragma: export

#include <voxen/error_condition.hpp>
#include <voxen/exception.hpp>
#include <voxen/land/chunk_key.hpp>
#include <voxen/svc/engine.hpp>

#include <catch2/matchers/catch_matchers_predicate.hpp>

#include <string>
#include <system_error>

namespace voxen::test
{

// Creates `Engine` with test suite app info and no command line. Only the core
// services are registered, tests register any other services they need themselves.
svc::Engine::Ptr createEngine();

// Usage: `CHECK_THROWS_MATCHES(<expr>, Exception, errcExceptionMatcher(VoxenErrc::<code>))`
inline auto errcExceptionMatcher(VoxenErrc ec)
{
	return Catch::Matchers::Predicate<Exception>([ec](const Exception& ex) { return ex.error() == ec; });
}

inline auto errcExceptionMatcher(std::errc ec)
{
	return Catch::Matchers::Predicate<Exception>([ec](const Exception& ex) { return ex.error() == ec; });
}

} // namespace voxen::test

namespace Catch
{

template<>
struct StringMaker<voxen::land::ChunkKey> {
	static std::string convert(voxen::land::ChunkKey key);
};

} // namespace Catch
