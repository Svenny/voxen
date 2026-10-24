#include <tk/demangle.hpp>

#include "../test_common.hpp"

namespace tk
{

TEST_CASE("'demangle' handles basic names", "[tk::demangle]")
{
	CHECK(demangle("_Z1fv") == "f()");
	CHECK(demangle("_ZN2ns3fooEiPKc") == "ns::foo(int, char const*)");
	CHECK(
		demangle("_ZNSt6vectorIiSaIiEE9push_backERKi") == "std::vector<int, std::allocator<int>>::push_back(int const&)");
	CHECK(demangle("_ZZ4mainENK3$_0clEv") == "main::$_0::operator()() const");
}

TEST_CASE("'demangle' handles manglings libstdc++ fails on", "[tk::demangle]")
{
	// Template parameter declarations of Clang 17+:
	// `template<C T> void f(T)` with a concept `C`
	CHECK(demangle("_Z1fITk1CiEvT_") == "void f<int>(int)");
	// `template<auto N> void g()`
	CHECK(demangle("_Z1gITnDaLi1EEvv") == "void g<1>()");
	// `template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0> void h(T)`
	CHECK(demangle("_Z1hIiTnNSt9enable_ifIXsr3stdE13is_integral_vIT_EEiE4typeELi0EEvS1_") == "void h<int, 0>(int)");
	// Suffix of a ThinLTO-promoted local symbol
	CHECK(demangle("_ZN12_GLOBAL__N_15valueE.llvm.123") == "(anonymous namespace)::value (.llvm.123)");
}

TEST_CASE("'demangle' rejects invalid names", "[tk::demangle]")
{
	CHECK(demangle("") == std::nullopt);
	CHECK(demangle("main") == std::nullopt);
	// Valid type mangling but not a symbol name
	CHECK(demangle("i") == std::nullopt);
	CHECK(demangle("_Z") == std::nullopt);
	CHECK(demangle("_ZN2ns3fooE!") == std::nullopt);
}

} // namespace tk
