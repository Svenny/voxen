#include <voxen/util/hash.hpp>

#include "../../test_common.hpp"

namespace voxen
{

TEST_CASE("xxh64Fixed", "[voxen::hash]")
{
	// Compare against values from reference XXH64 implementation with seed 0
	CHECK(Hash::xxh64Fixed(0) == 0x34C96ACDCADB1BBB);

	CHECK(Hash::xxh64Fixed(0xC20369A413E28FC1) == 0xE887D97F3EFE7B44);
	CHECK(Hash::xxh64Fixed(0xC722205F1C53D89F) == 0x68BEC6640212567D);
	CHECK(Hash::xxh64Fixed(0x146AEAC22CD734F6) == 0xECFBB0C2A1E3E878);
	CHECK(Hash::xxh64Fixed(0x33AF2950D2E525EC) == 0x03760006CA050043);
	CHECK(Hash::xxh64Fixed(0x50745822FA9B4673) == 0x199F8B0904FA343A);
}

} // namespace voxen
