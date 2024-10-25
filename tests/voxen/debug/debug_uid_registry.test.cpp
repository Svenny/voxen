#include <voxen/debug/debug_uid_registry.hpp>

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

TEST_CASE_METHOD(TestFixture, "'debug_uid_registry' basic test case", "[voxen::debug::debug_uid_registry]")
{
	UidRegistry::registerLiteral(U1, "U1");
	UidRegistry::registerLiteral(U2, "U2");
	UidRegistry::registerLiteral(U3, "U3");
	UidRegistry::registerLiteral(U4, "U4");

	CHECK(UidRegistry::lookup(U1) == "U1");
	CHECK(UidRegistry::lookup(U2) == "U2");
	CHECK(UidRegistry::lookup(U3) == "U3");
	CHECK(UidRegistry::lookup(U4) == "U4");

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

	CHECK(UidRegistry::lookup(U1) == "uid1");
	CHECK(UidRegistry::lookup(U2) == "uid2");

	std::string str = "junk";
	UidRegistry::lookup(U3, str);
	CHECK(str == "uid3");
	UidRegistry::lookup(U4, str);
	CHECK(str == "uid4");

	UidRegistry::unregister(U1);
	UidRegistry::lookup(U1, str);
	CHECK(str == "");

	UidRegistry::unregister(U2);
	CHECK(UidRegistry::lookup(U2) == "");

	UidRegistry::unregister(U3);
	CHECK(UidRegistry::lookup(U3) == "");

	UidRegistry::unregister(U4);
	CHECK(UidRegistry::lookup(U4) == "");
}

TEST_CASE_METHOD(TestFixture, "'debug_uid_registry' test lookupOrPrint", "[voxen::debug::debug_uid_registry]")
{
	UidRegistry::registerLiteral(U1, "U1");
	UidRegistry::registerLiteral(U2, "c2b6fae1-a1aded58-0f054134-53d47bec");
	UidRegistry::registerLiteral(U3, "c5c74611-2d43b146-dc098141-b47700f8"); // not U3 value

	CHECK(UidRegistry::lookupOrPrint(U1) == "U1");
	CHECK(UidRegistry::lookupOrPrint(U2) == "c2b6fae1-a1aded58-0f054134-53d47bec");
	CHECK(UidRegistry::lookupOrPrint(U3) == "c5c74611-2d43b146-dc098141-b47700f8");
	CHECK(UidRegistry::lookupOrPrint(U4) == "8819c518-0260c91d-db31ab20-f0daee10");

	std::string str = "junk";
	UidRegistry::lookupOrPrint(U4, str);
	CHECK(str == "8819c518-0260c91d-db31ab20-f0daee10");
	UidRegistry::lookupOrPrint(U2, str);
	CHECK(str == "c2b6fae1-a1aded58-0f054134-53d47bec");
}

} // namespace voxen::debug
