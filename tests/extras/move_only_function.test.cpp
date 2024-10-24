#include <extras/move_only_function.hpp>

#include "../test_common.hpp"

// Test cases for `extras::move_only_function` are copied from GCC's libstdc++ test suite.
// I am not the author of this code.
namespace
{

using extras::move_only_function;

using std::in_place_type_t;
using std::invoke_result_t;
using std::is_constructible_v;
using std::is_copy_constructible_v;
using std::is_invocable_v;
using std::is_nothrow_constructible_v;
using std::is_nothrow_default_constructible_v;
using std::is_nothrow_invocable_v;
using std::is_nothrow_move_constructible_v;
using std::is_same_v;
using std::nullptr_t;

// Test cases copied from libstdc++ test suite (call.cc):
// https://github.com/gcc-mirror/gcc/blob/master/libstdc%2B%2B-v3/testsuite/20_util/move_only_function/call.cc

// Check return types
static_assert(is_same_v<void, invoke_result_t<move_only_function<void()>>>);
static_assert(is_same_v<int, invoke_result_t<move_only_function<int()>>>);
static_assert(is_same_v<int &, invoke_result_t<move_only_function<int &()>>>);

// With const qualifier
static_assert(!is_invocable_v<move_only_function<void()> const>);
static_assert(!is_invocable_v<move_only_function<void()> const &>);
static_assert(is_invocable_v<move_only_function<void() const>>);
static_assert(is_invocable_v<move_only_function<void() const> &>);
static_assert(is_invocable_v<move_only_function<void() const> const>);
static_assert(is_invocable_v<move_only_function<void() const> const &>);

// With no ref-qualifier
static_assert(is_invocable_v<move_only_function<void()>>);
static_assert(is_invocable_v<move_only_function<void()> &>);
static_assert(is_invocable_v<move_only_function<void() const>>);
static_assert(is_invocable_v<move_only_function<void() const> &>);
static_assert(is_invocable_v<move_only_function<void() const> const>);
static_assert(is_invocable_v<move_only_function<void() const> const &>);

// With & ref-qualifier
static_assert(!is_invocable_v<move_only_function<void() &>>);
static_assert(is_invocable_v<move_only_function<void() &> &>);
static_assert(is_invocable_v<move_only_function<void() const &>>);
static_assert(is_invocable_v<move_only_function<void() const &> &>);
static_assert(is_invocable_v<move_only_function<void() const &> const>);
static_assert(is_invocable_v<move_only_function<void() const &> const &>);

// With && ref-qualifier
static_assert(is_invocable_v<move_only_function<void() &&>>);
static_assert(!is_invocable_v<move_only_function<void() &&> &>);
static_assert(is_invocable_v<move_only_function<void() const &&>>);
static_assert(!is_invocable_v<move_only_function<void() const &&> &>);
static_assert(is_invocable_v<move_only_function<void() const &&> const>);
static_assert(!is_invocable_v<move_only_function<void() const &&> const &>);

// With noexcept-specifier
static_assert(!is_nothrow_invocable_v<move_only_function<void()>>);
static_assert(!is_nothrow_invocable_v<move_only_function<void() noexcept(false)>>);
static_assert(is_nothrow_invocable_v<move_only_function<void() noexcept>>);
static_assert(is_nothrow_invocable_v<move_only_function<void() & noexcept> &>);

TEST_CASE("'move_only_function' calls test case 1", "[extras::move_only_function]")
{
	struct F {
		int operator()() { return 0; }
		int operator()() const { return 1; }
	};

	move_only_function<int()> f0 { F {} };
	CHECK(f0() == 0);
	CHECK(std::move(f0)() == 0);

	move_only_function<int() const> f1 { F {} };
	CHECK(f1() == 1);
	CHECK(std::as_const(f1)() == 1);
	CHECK(std::move(f1)() == 1);
	CHECK(std::move(std::as_const(f1))() == 1);

	move_only_function<int() &> f2 { F {} };
	CHECK(f2() == 0);
	// Not rvalue-callable: std::move(f2)()

	move_only_function<int() const &> f3 { F {} };
	CHECK(f3() == 1);
	CHECK(std::as_const(f3)() == 1);
	CHECK(std::move(f3)() == 1);
	CHECK(std::move(std::as_const(f3))() == 1);

	move_only_function<int() &&> f4 { F {} };
	// Not lvalue-callable: f4()
	CHECK(std::move(f4)() == 0);

	move_only_function<int() const &&> f5 { F {} };
	// Not lvalue-callable: f5()
	CHECK(std::move(f5)() == 1);
	CHECK(std::move(std::as_const(f5))() == 1);
}

TEST_CASE("'move_only_function' calls test case 2", "[extras::move_only_function]")
{
	struct F {
		int operator()() & { return 0; }
		int operator()() && { return 1; }
	};

	move_only_function<int()> f0 { F {} };
	CHECK(f0() == 0);
	CHECK(std::move(f0)() == 0);

	move_only_function<int() &&> f1 { F {} };
	// Not lvalue callable: f1()
	CHECK(std::move(f1)() == 1);

	move_only_function<int() &> f2 { F {} };
	CHECK(f2() == 0);
	// Not rvalue-callable: std::move(f2)()
}

TEST_CASE("'move_only_function' calls test case 3", "[extras::move_only_function]")
{
	struct F {
		int operator()() const & { return 0; }
		int operator()() && { return 1; }
	};

	move_only_function<int()> f0 { F {} };
	CHECK(f0() == 0);
	CHECK(std::move(f0)() == 0);

	move_only_function<int() &&> f1 { F {} };
	// Not lvalue callable: f1()
	CHECK(std::move(f1)() == 1);

	move_only_function<int() const> f2 { F {} };
	CHECK(f2() == 0);
	CHECK(std::as_const(f2)() == 0);
	CHECK(std::move(f2)() == 0);
	CHECK(std::move(std::as_const(f2))() == 0);

	move_only_function<int() const &&> f3 { F {} };
	// Not lvalue callable: f3()
	CHECK(std::move(f3)() == 0);
	CHECK(std::move(std::as_const(f3))() == 0);

	move_only_function<int() const &> f4 { F {} };
	CHECK(f4() == 0);
	CHECK(std::as_const(f4)() == 0);
	// Not rvalue-callable: std::move(f4)()
}

TEST_CASE("'move_only_function' calls test case 4", "[extras::move_only_function]")
{
	struct F {
		int operator()() & { return 0; }
		int operator()() && { return 1; }
		int operator()() const & { return 2; }
		int operator()() const && { return 3; }
	};

	move_only_function<int()> f0 { F {} };
	CHECK(f0() == 0);
	CHECK(std::move(f0)() == 0);

	move_only_function<int() &> f1 { F {} };
	CHECK(f1() == 0);
	// Not rvalue-callable: std::move(f1)()

	move_only_function<int() &&> f2 { F {} };
	// Not lvalue callable: f2()
	CHECK(std::move(f2)() == 1);

	move_only_function<int() const> f3 { F {} };
	CHECK(f3() == 2);
	CHECK(std::as_const(f3)() == 2);
	CHECK(std::move(f3)() == 2);
	CHECK(std::move(std::as_const(f3))() == 2);

	move_only_function<int() const &> f4 { F {} };
	CHECK(f4() == 2);
	CHECK(std::as_const(f4)() == 2);
	// Not rvalue-callable: std::move(f4)()

	move_only_function<int() const &&> f5 { F {} };
	// Not lvalue callable: f5()
	CHECK(std::move(f5)() == 3);
	CHECK(std::move(std::as_const(f5))() == 3);
}

struct Incomplete;

TEST_CASE("'move_only_function' with incomplete types", "[extras::move_only_function]")
{
	move_only_function<void(Incomplete)> f1;
	move_only_function<void(Incomplete &)> f2;
	move_only_function<void(Incomplete &&)> f3;
}

// Test cases copied from libstdc++ test suite (cons.cc):
// https://github.com/gcc-mirror/gcc/blob/master/libstdc%2B%2B-v3/testsuite/20_util/move_only_function/cons.cc

static_assert(is_nothrow_default_constructible_v<move_only_function<void()>>);
static_assert(is_nothrow_constructible_v<move_only_function<void()>, nullptr_t>);
static_assert(is_nothrow_move_constructible_v<move_only_function<void()>>);
static_assert(!is_copy_constructible_v<move_only_function<void()>>);

static_assert(is_constructible_v<move_only_function<void()>, void()>);
static_assert(is_constructible_v<move_only_function<void()>, void (&)()>);
static_assert(is_constructible_v<move_only_function<void()>, void (*)()>);
static_assert(is_constructible_v<move_only_function<void()>, int()>);
static_assert(is_constructible_v<move_only_function<void()>, int (&)()>);
static_assert(is_constructible_v<move_only_function<void()>, int (*)()>);
static_assert(!is_constructible_v<move_only_function<void()>, void(int)>);
static_assert(is_constructible_v<move_only_function<void(int)>, void(int)>);

static_assert(is_constructible_v<move_only_function<void(int)>, in_place_type_t<void (*)(int)>, void(int)>);

static_assert(is_constructible_v<move_only_function<void()>, void() noexcept>);
static_assert(is_constructible_v<move_only_function<void() noexcept>, void() noexcept>);
static_assert(!is_constructible_v<move_only_function<void() noexcept>, void()>);

struct Q {
	void operator()() const &;
	void operator()() &&;
};

static_assert(is_constructible_v<move_only_function<void()>, Q>);
static_assert(is_constructible_v<move_only_function<void() const>, Q>);
static_assert(is_constructible_v<move_only_function<void() &>, Q>);
static_assert(is_constructible_v<move_only_function<void() const &>, Q>);
static_assert(is_constructible_v<move_only_function<void() &&>, Q>);
static_assert(is_constructible_v<move_only_function<void() const &&>, Q>);

struct R {
	void operator()() &;
	void operator()() &&;
};

static_assert(is_constructible_v<move_only_function<void()>, R>);
static_assert(is_constructible_v<move_only_function<void() &>, R>);
static_assert(is_constructible_v<move_only_function<void() &&>, R>);
static_assert(!is_constructible_v<move_only_function<void() const>, R>);
static_assert(!is_constructible_v<move_only_function<void() const &>, R>);
static_assert(!is_constructible_v<move_only_function<void() const &&>, R>);

// The following nothrow-constructible guarantees are a GCC extension,
// not required by the standard.

static_assert(is_nothrow_constructible_v<move_only_function<void()>, void()>);
static_assert(is_nothrow_constructible_v<move_only_function<void(int)>, in_place_type_t<void (*)(int)>, void(int)>);

// These types are all small and nothrow move constructible
struct F {
	void operator()();
};
struct G {
	void operator()() const;
};
static_assert(is_nothrow_constructible_v<move_only_function<void()>, F>);
static_assert(is_nothrow_constructible_v<move_only_function<void()>, G>);
static_assert(is_nothrow_constructible_v<move_only_function<void() const>, G>);

struct H {
	H(int);
	H(int, int) noexcept;
	void operator()() noexcept;
};
static_assert(is_nothrow_constructible_v<move_only_function<void()>, H>);
static_assert(is_nothrow_constructible_v<move_only_function<void() noexcept>, H>);
static_assert(!is_nothrow_constructible_v<move_only_function<void() noexcept>, in_place_type_t<H>, int>);
static_assert(is_nothrow_constructible_v<move_only_function<void() noexcept>, in_place_type_t<H>, int, int>);

struct I {
	I(int, const char *) {}
	I(std::initializer_list<char>) {}
	int operator()() const noexcept { return 0; }
};

static_assert(is_constructible_v<move_only_function<void()>, std::in_place_type_t<I>, int, const char *>);
static_assert(is_constructible_v<move_only_function<void()>, std::in_place_type_t<I>, std::initializer_list<char>>);

TEST_CASE("'move_only_function' instantiations", "[extras::move_only_function]")
{
	// Instantiate the constructor bodies
	move_only_function<void()> f0;
	move_only_function<void()> f1(nullptr);
	move_only_function<void()> f2(I(1, "two"));
	move_only_function<void()> f3(std::in_place_type<I>, 3, "four");
	move_only_function<void()> f4(std::in_place_type<I>, // PR libstdc++/102825
		{ 'P', 'R', '1', '0', '2', '8', '2', '5' });
	auto f5 = std::move(f4);
	f4 = std::move(f5);
}

// Test cases copied from libstdc++ test suite (move.cc):
// https://github.com/gcc-mirror/gcc/blob/master/libstdc%2B%2B-v3/testsuite/20_util/move_only_function/move.cc

TEST_CASE("'move_only_function' moves test case 1", "[extras::move_only_function]")
{
	// Small type with non-throwing move constructor. Not allocated on the heap.
	struct F {
		F() = default;
		F(const F &f) : counters(f.counters) { ++counters.copy; }
		F(F &&f) noexcept : counters(f.counters) { ++counters.move; }

		F &operator=(F &&) = delete;

		struct Counters {
			int copy = 0;
			int move = 0;
		} counters;

		const Counters &operator()() const { return counters; }
	};

	F f;
	move_only_function<const F::Counters &() const> m1(f);
	CHECK(m1().copy == 1);
	CHECK(m1().move == 0);

	// This will move construct a new target object and destroy the old one:
	auto m2 = std::move(m1);
	CHECK(m1 == nullptr);
	CHECK(m2 != nullptr);
	CHECK(m2().copy == 1);
	CHECK(m2().move == 1);

	m1 = std::move(m2);
	CHECK(m1 != nullptr);
	CHECK(m2 == nullptr);
	CHECK(m1().copy == 1);
	CHECK(m1().move == 2);

	m2 = std::move(f);
	CHECK(m2().copy == 0);
	CHECK(m2().move == 2); // Move construct target object, then swap into m2.
	const int moves = m1().move + m2().move;
	// This will do three moves:
	swap(m1, m2);
	CHECK(m1().copy == 0);
	CHECK(m2().copy == 1);
	CHECK((m1().move + m2().move) == (moves + 3));
}

TEST_CASE("'move_only_function' moves test case 2", "[extras::move_only_function]")
{
	// Move constructor is potentially throwing. Allocated on the heap.
	struct F {
		F() = default;
		F(const F &f) noexcept : counters(f.counters) { ++counters.copy; }
		F(F &&f) noexcept(false) : counters(f.counters) { ++counters.move; }

		F &operator=(F &&) = delete;

		struct Counters {
			int copy = 0;
			int move = 0;
		} counters;

		Counters operator()() const noexcept { return counters; }
	};

	F f;
	move_only_function<F::Counters() const> m1(f);
	CHECK(m1().copy == 1);
	CHECK(m1().move == 0);

	// The target object is on the heap so this just moves a pointer:
	auto m2 = std::move(m1);
	CHECK(m1 == nullptr);
	CHECK(m2 != nullptr);
	CHECK(m2().copy == 1);
	CHECK(m2().move == 0);

	m1 = std::move(m2);
	CHECK(m1 != nullptr);
	CHECK(m2 == nullptr);
	CHECK(m1().copy == 1);
	CHECK(m1().move == 0);

	m2 = std::move(f);
	CHECK(m2().copy == 0);
	CHECK(m2().move == 1);
	const int moves = m1().move + m2().move;
	// This just swaps the pointers, so no moves:
	swap(m1, m2);
	CHECK(m1().copy == 0);
	CHECK(m2().copy == 1);
	CHECK((m1().move + m2().move) == moves);
}

} // anonymous namespace
