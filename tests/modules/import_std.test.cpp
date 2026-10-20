// EXPERIMENT: `import std;` together with our modules and textual includes

#include "../test_common.hpp"

import std;
import modstd;
import tk.rng;

namespace
{

TEST_CASE("import std works", "[modules::import_std]")
{
	std::vector<std::string> words { "import", "std" };
	CHECK(std::ranges::fold_left(words, std::string(), std::plus()) == "importstd");
	CHECK(std::format("{}-{}", 1, 2) == "1-2");

	// Module unit built with `import std;` in place of the global module fragment
	CHECK(modstd::joined(words, ' ') == "import std");
	CHECK(modstd::firstRandom(42) == tk::rng::SplitMix64(42)());
}

} // namespace
