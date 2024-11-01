#include <voxen/debug/uid_registry.hpp>

#include "../../voxen_test_common.hpp"

#include <extras/defer.hpp>

namespace voxen::debug
{

namespace
{

constexpr UID U1("1fc82db5-ea75f28a-c21c223b-10663645");
constexpr UID U2("c2b6fae1-a1aded58-0f054134-53d47bec");
constexpr UID U3("dc098141-b47700f8-2d43b146-c5c74611");
constexpr UID U4("8819c518-0260c91d-db31ab20-f0daee10");

struct TestFixture {
	~TestFixture()
	{
		// Clean up after these test cases, registry state is global
		UidRegistry::unregister(U1);
		UidRegistry::unregister(U2);
		UidRegistry::unregister(U3);
		UidRegistry::unregister(U4);
	}
};

} // namespace

TEST_CASE_METHOD(TestFixture, "'UidRegistry' basic test case", "[voxen::debug::uid_registry]")
{
	UidRegistry::registerLiteral(U1, "U1");
	UidRegistry::registerLiteral(U2, "U2");
	UidRegistry::registerLiteral(U3, "U3");
	UidRegistry::registerLiteral(U4, "U4");

	CHECK(UidRegistry::lookup(U1, UidRegistry::FORMAT_STRING_ONLY) == "U1");
	CHECK(UidRegistry::lookup(U2, UidRegistry::FORMAT_STRING_ONLY) == "U2");
	CHECK(UidRegistry::lookup(U3, UidRegistry::FORMAT_STRING_ONLY) == "U3");
	CHECK(UidRegistry::lookup(U4, UidRegistry::FORMAT_STRING_ONLY) == "U4");

	{
		std::string str;

		str = "uid1";
		UidRegistry::registerString(U1, str);

		str = "uid2";
		UidRegistry::registerString(U2, str);

		str = "uid3";
		UidRegistry::registerString(U3, str);

		str = "uid4";
		UidRegistry::registerString(U4, str);
	}

	CHECK(UidRegistry::lookup(U1, UidRegistry::FORMAT_STRING_ONLY) == "uid1");
	CHECK(UidRegistry::lookup(U2, UidRegistry::FORMAT_STRING_ONLY) == "uid2");

	std::string str = "junk";
	UidRegistry::lookup(U3, str, UidRegistry::FORMAT_STRING_ONLY);
	CHECK(str == "uid3");
	UidRegistry::lookup(U4, str, UidRegistry::FORMAT_STRING_ONLY);
	CHECK(str == "uid4");

	UidRegistry::unregister(U1);
	UidRegistry::lookup(U1, str, UidRegistry::FORMAT_STRING_ONLY);
	CHECK(str == "");

	UidRegistry::unregister(U2);
	CHECK(UidRegistry::lookup(U2, UidRegistry::FORMAT_STRING_ONLY) == "");

	UidRegistry::unregister(U3);
	CHECK(UidRegistry::lookup(U3, UidRegistry::FORMAT_STRING_ONLY) == "");

	UidRegistry::unregister(U4);
	CHECK(UidRegistry::lookup(U4, UidRegistry::FORMAT_STRING_ONLY) == "");
}

TEST_CASE_METHOD(TestFixture, "'UidRegistry' test lookup formats", "[voxen::debug::uid_registry]")
{
	UidRegistry::registerLiteral(U1, "U1");
	UidRegistry::registerLiteral(U2, "c2b6fae1-a1aded58-0f054134-53d47bec");
	UidRegistry::registerLiteral(U3, "c5c74611-2d43b146-dc098141-b47700f8"); // not U3 value

	CHECK(UidRegistry::lookup(U1) == "U1 (1fc82db5-ea75f28a-c21c223b-10663645)");
	CHECK(UidRegistry::lookup(U2) == "c2b6fae1-a1aded58-0f054134-53d47bec (c2b6fae1-a1aded58-0f054134-53d47bec)");
	CHECK(UidRegistry::lookup(U3) == "c5c74611-2d43b146-dc098141-b47700f8 (dc098141-b47700f8-2d43b146-c5c74611)");
	CHECK(UidRegistry::lookup(U4) == "8819c518-0260c91d-db31ab20-f0daee10");

	UidRegistry::Format f = UidRegistry::FORMAT_STRING_AND_UID;
	CHECK(UidRegistry::lookup(U1, f) == "U1 (1fc82db5-ea75f28a-c21c223b-10663645)");
	CHECK(UidRegistry::lookup(U2, f) == "c2b6fae1-a1aded58-0f054134-53d47bec (c2b6fae1-a1aded58-0f054134-53d47bec)");
	CHECK(UidRegistry::lookup(U3, f) == "c5c74611-2d43b146-dc098141-b47700f8 (dc098141-b47700f8-2d43b146-c5c74611)");
	CHECK(UidRegistry::lookup(U4, f) == "8819c518-0260c91d-db31ab20-f0daee10");

	f = UidRegistry::FORMAT_STRING_OR_UID;
	CHECK(UidRegistry::lookup(U1, f) == "U1");
	CHECK(UidRegistry::lookup(U2, f) == "c2b6fae1-a1aded58-0f054134-53d47bec");
	CHECK(UidRegistry::lookup(U3, f) == "c5c74611-2d43b146-dc098141-b47700f8");
	CHECK(UidRegistry::lookup(U4, f) == "8819c518-0260c91d-db31ab20-f0daee10");

	f = UidRegistry::FORMAT_STRING_ONLY;
	CHECK(UidRegistry::lookup(U1, f) == "U1");
	CHECK(UidRegistry::lookup(U2, f) == "c2b6fae1-a1aded58-0f054134-53d47bec");
	CHECK(UidRegistry::lookup(U3, f) == "c5c74611-2d43b146-dc098141-b47700f8");
	CHECK(UidRegistry::lookup(U4, f) == "");

	std::string str = "junk";
	UidRegistry::lookup(U4, str, UidRegistry::FORMAT_STRING_OR_UID);
	CHECK(str == "8819c518-0260c91d-db31ab20-f0daee10");
	UidRegistry::lookup(U2, str, UidRegistry::FORMAT_STRING_OR_UID);
	CHECK(str == "c2b6fae1-a1aded58-0f054134-53d47bec");
}

} // namespace voxen::debug
