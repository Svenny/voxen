#include <extras/move_only_function.hpp>

#include "../test_common.hpp"

namespace extras
{

// copy from:
// https://github.com/gcc-mirror/gcc/blob/master/libstdc%2B%2B-v3/testsuite/20_util/move_only_function/call.cc
// https://github.com/gcc-mirror/gcc/blob/master/libstdc%2B%2B-v3/testsuite/20_util/move_only_function/cons.cc
// https://github.com/gcc-mirror/gcc/blob/master/libstdc%2B%2B-v3/testsuite/20_util/move_only_function/move.cc

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

// Constructible asserts
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

TEST_CASE("'move_only_function' can be created from various objects", "[extras::move_only_function]") {}

} // namespace extras
