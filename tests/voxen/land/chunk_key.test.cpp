#include <voxen/land/chunk_key.hpp>

#include "../../test_common.hpp"

namespace voxen::land
{

TEST_CASE("'ChunkKey' sanity check", "[voxen::land::chunk_key]")
{
	ChunkKey ck(glm::ivec3(8, 4, 2), 1);
	CHECK(ck.base() == glm::ivec3(8, 4, 2));
	CHECK(ck.scaleLog2() == 1);
	CHECK(ck.scaleMultiplier() == 2);

	ChunkKey parent = ck.parentLodKey();
	CHECK(parent.base() == glm::ivec3(8, 4, 0));
	CHECK(parent.scaleLog2() == 2);

	ChunkKey parent2 = parent.parentLodKey();
	CHECK(parent2.base() == glm::ivec3(8, 0, 0));
	CHECK(parent2.scaleLog2() == 3);

	// Round-trip packing
	CHECK(ChunkKey(ck.packed()) == ck);
	CHECK(ChunkKey(parent.packed()) == parent);
	CHECK(ChunkKey(parent2.packed()) == parent2);
}

TEST_CASE("'ChunkKey' with negative values", "[voxen::land::chunk_key]")
{
	ChunkKey ck(glm::ivec3(-8, -1, -3), 0);
	CHECK(ck.base() == glm::ivec3(-8, -1, -3));
	CHECK(ck.scaleLog2() == 0);

	ChunkKey parent = ck.parentLodKey();
	CHECK(parent.base() == glm::ivec3(-8, -2, -4));
	CHECK(parent.scaleLog2() == 1);

	ChunkKey parent2 = parent.parentLodKey();
	CHECK(parent2.base() == glm::ivec3(-8, -4, -4));
	CHECK(parent2.scaleLog2() == 2);

	// Round-trip packing
	CHECK(ChunkKey(ck.packed()) == ck);
	CHECK(ChunkKey(parent.packed()) == parent);
	CHECK(ChunkKey(parent2.packed()) == parent2);
}

} // namespace voxen::land
