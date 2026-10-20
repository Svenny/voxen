// EXPERIMENT: a plain (non-module) TU mixing textual includes of std, fmt, Vulkan
// and our own headers with imports of modules from static and shared libraries

#include <modshared/api.hpp>

#include <m/format.hpp>

#include "../test_common.hpp"

#include <vulkan/vulkan.h>

#include <fmt/format.h>

#include <cstdint>
#include <string>
#include <vector>

import m;
import modstatic;
import tk;

namespace
{

TEST_CASE("Modules from shared and static libraries work together", "[modules]")
{
	const int32_t calls_before = modshared::g_calls;

	modstatic::NamedCounter counter("named");
	counter.add(2);
	counter.add(3);
	CHECK(counter.value() == 5);

	// Virtual call through a base exported from the shared library
	const modshared::Counter &base = counter;
	CHECK(base.name() == "named");
	CHECK(modshared::Counter().name() == "Counter");

	// Types from `m`, entities are the same through the header and the import
	const m::float3 v = modshared::scaled(m::float3(1.0f, 2.0f, 3.0f), 2.0f);
	CHECK(v == m::float3(2.0f, 4.0f, 6.0f));
	CHECK(fmt::format("{}", v) == fmt::format("{}", m::float3(2.0f, 4.0f, 6.0f)));

	// Same generator through the module of a shared library and directly
	CHECK(modshared::seededRandom(42) == tk::rng::SplitMix64(42)());

	CHECK(modshared::g_calls == calls_before + 4);

	CHECK(modshared::twice(21) == 42);
	CHECK(modshared::answer() == 42);
	CHECK(MODSHARED_SQUARE(modstatic::sumOfSquares(1, 2)) == 25);

	// Something from textually included headers, to be sure they coexist
	std::vector<VkExtent2D> extents { { 1, 2 } };
	CHECK(extents[0].height == 2);
}

} // namespace
