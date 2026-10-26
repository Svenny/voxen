#include <tk/uid.hpp>

#include "../test_common.hpp"

#include <tk/format.hpp>

#include <fmt/format.h>

#include <cstdint>
#include <string_view>
#include <unordered_set>

namespace tk
{

namespace
{

constexpr UID TEST_UID("c1bf2846-ff1f9f34-a0abff03-e68abb9b");

} // namespace

TEST_CASE("'UID' string constructor", "[tk::uid]")
{
	STATIC_CHECK(TEST_UID == UID(0xc1bf2846ff1f9f34, 0xa0abff03e68abb9b));
	STATIC_CHECK(UID("00000000-00000000-00000000-00000000") == UID());
	STATIC_CHECK(UID("ffffffff-ffffffff-ffffffff-ffffffff") == UID(UINT64_MAX, UINT64_MAX));
}

TEST_CASE("'UID' string round-trip", "[tk::uid]")
{
	char chars[UID::CHAR_REPR_LENGTH];
	TEST_UID.toChars(chars);
	CHECK(std::string_view(chars) == "c1bf2846-ff1f9f34-a0abff03-e68abb9b");
	CHECK(UID::fromChars(chars) == TEST_UID);
	CHECK(fmt::format("{}", TEST_UID) == "c1bf2846-ff1f9f34-a0abff03-e68abb9b");

	UID(0x0000000100000002, 0x00000003000000a0).toChars(chars);
	CHECK(std::string_view(chars) == "00000001-00000002-00000003-000000a0");

	const UID random = UID::generateRandom();
	random.toChars(chars);
	CHECK(UID::fromChars(chars) == random);
}

TEST_CASE("'UID::fromChars' rejects invalid input", "[tk::uid]")
{
	CHECK_FALSE(UID::fromChars("").has_value());
	// Wrong length
	CHECK_FALSE(UID::fromChars("c1bf2846-ff1f9f34-a0abff03-e68abb9").has_value());
	CHECK_FALSE(UID::fromChars("c1bf2846-ff1f9f34-a0abff03-e68abb9b0").has_value());
	// Wrong separators
	CHECK_FALSE(UID::fromChars("c1bf2846-ff1f9f34-a0abff03@e68abb9b").has_value());
	CHECK_FALSE(UID::fromChars("c1bf2846ff1f9f34-a0abff03-e68abb9b-").has_value());
	// Non-hex and uppercase hex digits
	CHECK_FALSE(UID::fromChars("c1bf2846-ff1f9f34-a0abff03-#68abb9b").has_value());
	CHECK_FALSE(UID::fromChars("c1bf2846-ff1f9f34-a0abff03-e68ABB9b").has_value());

	CHECK_FALSE(UID::tryParse("g1bf2846-ff1f9f34-a0abff03-e68abb9b").has_value());
}

TEST_CASE("'deriveUid' keeps its bit pattern", "[tk::uid]")
{
	// Derived UIDs may be persisted, the result must never change
	STATIC_CHECK(deriveUid(TEST_UID, 0) == UID(0xc1bf2846ff1f9f34, 0x89ef54f406f236b2));
	STATIC_CHECK(deriveUid(TEST_UID, 42) == UID(0xc1bf2846ff1f9f34, 0x3b82f70d0deae5d0));
}

TEST_CASE("'UID' hashing", "[tk::uid]")
{
	std::unordered_set<UID> set;
	set.insert(TEST_UID);
	set.insert(deriveUid(TEST_UID, 1));
	set.insert(TEST_UID);
	CHECK(set.size() == 2);
}

} // namespace tk
