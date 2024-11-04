#include <voxen/util/concentric_octahedra_walker.hpp>

#include "../../voxen_test_common.hpp"

namespace voxen
{

TEST_CASE("'ConcentricOctahedraWalker' with radius 0", "[voxen::concentric_octahedra_walker]")
{
	ConcentricOctahedraWalker walker(0);

	CHECK(!walker.wrappedAround());

	for (int i = 0; i < 3; i++) {
		CHECK(walker.step() == glm::ivec3(0));
	}

	CHECK(walker.wrappedAround());
}

TEST_CASE("'ConcentricOctahedraWalker' with radius 1", "[voxen::concentric_octahedra_walker]")
{
	ConcentricOctahedraWalker walker(1);

	// Radius 0
	CHECK(walker.step() == glm::ivec3(0, 0, 0));
	// Radius 1
	CHECK(walker.step() == glm::ivec3(-1, 0, 0));
	CHECK(walker.step() == glm::ivec3(0, 0, -1));
	CHECK(walker.step() == glm::ivec3(0, 1, 0));
	CHECK(walker.step() == glm::ivec3(0, -1, 0));
	CHECK(walker.step() == glm::ivec3(0, 0, 1));
	CHECK(!walker.wrappedAround());
	CHECK(walker.step() == glm::ivec3(1, 0, 0));
	CHECK(walker.wrappedAround());
	// Again
	CHECK(walker.step() == glm::ivec3(0, 0, 0));
	CHECK(walker.step() == glm::ivec3(-1, 0, 0));
}

TEST_CASE("'ConcentricOctahedraWalker' with radius 2", "[voxen::concentric_octahedra_walker]")
{
	ConcentricOctahedraWalker walker(2);

	// Radius 0
	CHECK(walker.step() == glm::ivec3(0, 0, 0));
	// Radius 1
	CHECK(walker.step() == glm::ivec3(-1, 0, 0));
	CHECK(walker.step() == glm::ivec3(0, 0, -1));
	CHECK(walker.step() == glm::ivec3(0, 1, 0));
	CHECK(walker.step() == glm::ivec3(0, -1, 0));
	CHECK(walker.step() == glm::ivec3(0, 0, 1));
	CHECK(walker.step() == glm::ivec3(1, 0, 0));
	// Radius 2
	CHECK(walker.step() == glm::ivec3(-2, 0, 0));
	CHECK(walker.step() == glm::ivec3(-1, 0, -1));
	CHECK(walker.step() == glm::ivec3(-1, 1, 0));
	CHECK(walker.step() == glm::ivec3(-1, -1, 0));
	CHECK(walker.step() == glm::ivec3(-1, 0, 1));
	CHECK(walker.step() == glm::ivec3(0, 0, -2));
	CHECK(walker.step() == glm::ivec3(0, 1, -1));
	CHECK(walker.step() == glm::ivec3(0, -1, -1));
	CHECK(walker.step() == glm::ivec3(0, 2, 0));
	CHECK(walker.step() == glm::ivec3(0, -2, 0));
	CHECK(walker.step() == glm::ivec3(0, 1, 1));
	CHECK(walker.step() == glm::ivec3(0, -1, 1));
	CHECK(walker.step() == glm::ivec3(0, 0, 2));
	CHECK(walker.step() == glm::ivec3(1, 0, -1));
	CHECK(walker.step() == glm::ivec3(1, 1, 0));
	CHECK(walker.step() == glm::ivec3(1, -1, 0));
	CHECK(walker.step() == glm::ivec3(1, 0, 1));
	CHECK(!walker.wrappedAround());
	CHECK(walker.step() == glm::ivec3(2, 0, 0));
	CHECK(walker.wrappedAround());
	// Again
	CHECK(walker.step() == glm::ivec3(0, 0, 0));
}

TEST_CASE("'ConcentricOctahedraWalker' with radius 3", "[voxen::concentric_octahedra_walker]")
{
	ConcentricOctahedraWalker walker(3);

	// Skip results for radii 0 (1 result), 1 (6 results), 2 (18 results)
	for (int i = 0; i < 25; i++) {
		walker.step();
	}

	// First results of radius 3
	CHECK(walker.step() == glm::ivec3(-3, 0, 0));
	CHECK(walker.step() == glm::ivec3(-2, 0, -1));

	// Skip more results (total 38 results for radius 3)
	for (int i = 0; i < 34; i++) {
		walker.step();
	}

	// Last results of radius 3
	CHECK(walker.step() == glm::ivec3(2, 0, 1));
	CHECK(!walker.wrappedAround());
	CHECK(walker.step() == glm::ivec3(3, 0, 0));
	CHECK(walker.wrappedAround());

	// Again
	CHECK(walker.step() == glm::ivec3(0, 0, 0));
}

} // namespace voxen
