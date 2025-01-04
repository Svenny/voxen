#include <extras/enum_utils.hpp>

#include "../test_common.hpp"

namespace
{

enum Unscoped : uint32_t {
	UEmpty = 0,
	U4 = 1u << 4,
	U5 = 1u << 5,
	U10 = 1u << 10,
	U11 = 1u << 11,
	U22 = 1u << 22,
	U23 = 1u << 23,
};

enum class Scoped : uint64_t {
	S1 = 1ull << 1,
	S5 = 1ull << 5,
	S10 = 1ull << 10,
	S11 = 1ull << 11,
	S12 = 1ull << 12,
	S36 = 1ull << 36,
};

using UFlags = extras::enum_flags<Unscoped>;
using SFlags = extras::enum_flags<Scoped>;

} // namespace

// Explicitly instantiate to compile every function, even if we don't touch some
namespace extras
{

template struct extras::enum_flags<Unscoped>;
template struct extras::enum_flags<Scoped>;

} // namespace extras

TEST_CASE("'enum_flags' test with unscoped enum", "[extras::enum_utils]")
{
	UFlags f1 { UEmpty, U4, U10, U22 };
	UFlags f2 { Unscoped::U4, Unscoped::U11, U22 };
	UFlags f3 { U22, U11, UEmpty, UEmpty, U11, U5 };

	CHECK(SFlags {}.empty());

	CHECK(f1.test(U4));
	CHECK(!f1.test(U5));
	CHECK(f1.test_all(f1));
	CHECK(!f1.test_all(f2));
	CHECK(f1.test_any(f3));

	CHECK(UFlags { UEmpty, UEmpty }.empty());
	CHECK(f1.test(UEmpty));
	CHECK(f1.test_all(UFlags {}));
	CHECK(!f1.test_any(UFlags {}));

	CHECK(f2 == f2);
	CHECK(!(f2 == f3));
	CHECK(f3 != f2);
	CHECK(!(f3 != f3));

	CHECK((f1 | UEmpty) == f1);
	CHECK((f1 | U11) == UFlags { U4, U10, U11, U22 });
	CHECK((f1 | f2) == UFlags { U4, U10, U11, U22 });

	CHECK((f1 & UEmpty).empty());
	CHECK((f1 & U22) == UFlags { U22 });
	CHECK((f1 & f2) == UFlags { U4, U22 });

	CHECK((f1 ^ UEmpty) == f1);
	CHECK((f1 ^ f3) == UFlags { U4, U5, U10, U11 });
}

TEST_CASE("'enum_flags' test with scoped enum", "[extras::enum_utils]")
{
	SFlags f1 { Scoped::S1 };
	SFlags f2 { Scoped::S1, Scoped::S12 };
	SFlags f3 { Scoped::S10, Scoped::S12, Scoped::S36, Scoped::S5 };

	CHECK(SFlags().empty());
	CHECK(SFlags { Scoped::S1 }.test(Scoped::S1));
	CHECK(!SFlags { Scoped::S1 }.test(Scoped::S5));

	f1.clear();
	CHECK(f1.empty());
	f1.set(Scoped::S1);
	CHECK(f1.test(Scoped::S1));

	CHECK(!(~f1).test(Scoped::S1));
	CHECK((~f1).test(Scoped::S36));

	f1 |= f2;
	CHECK(f1.test(Scoped::S12));
	f1 ^= f2;
	CHECK(f1.empty());
	f3 &= f2;
	CHECK(f3.test_any(f2));
	CHECK(f3.test(Scoped::S12));
	CHECK(!f3.test(Scoped::S10));

	SFlags s4;
	s4.set(Scoped::S10);
	s4.unset(Scoped::S5);
	CHECK(s4 == SFlags(Scoped::S10));
	s4.unset(Scoped::S10);
	CHECK(s4.empty());
}
