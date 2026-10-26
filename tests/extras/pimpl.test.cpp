#include <extras/pimpl.hpp>

#include "../test_common.hpp"

#include <string>
#include <type_traits>
#include <utility>

namespace
{

struct Counters {
	int copies = 0;
	int moves = 0;
	int copy_assigns = 0;
	int move_assigns = 0;
	int destructions = 0;
};

struct Tracked {
	Tracked(Counters &c, std::string v) : counters(&c), value(std::move(v)) {}
	Tracked(const Tracked &other) : counters(other.counters), value(other.value) { counters->copies++; }
	Tracked(Tracked &&other) noexcept : counters(other.counters), value(std::move(other.value)) { counters->moves++; }

	Tracked &operator=(const Tracked &other)
	{
		value = other.value;
		counters->copy_assigns++;
		return *this;
	}

	Tracked &operator=(Tracked &&other) noexcept
	{
		value = std::move(other.value);
		counters->move_assigns++;
		return *this;
	}

	~Tracked() { counters->destructions++; }

	Counters *counters;
	std::string value;
};

using P = extras::pimpl<Tracked, 64, 8>;

// Construction from arguments is explicit, no implicit conversions into `pimpl`
static_assert(!std::is_convertible_v<Counters &, P>);
static_assert(std::is_copy_constructible_v<P> && std::is_nothrow_move_constructible_v<P>);
static_assert(std::is_copy_assignable_v<P> && std::is_nothrow_move_assignable_v<P>);

} // namespace

TEST_CASE("'pimpl' constructs, copies, moves and destroys the object", "[extras::pimpl]")
{
	Counters c;

	{
		P a(c, "a");
		CHECK(a->value == "a");

		// Non-const lvalue goes to the copy constructor, not the forwarding one
		P b(a);
		CHECK(b->value == "a");
		CHECK(c.copies == 1);

		const P &ca = a;
		P d(ca);
		CHECK(c.copies == 2);

		P e(std::move(b));
		CHECK(e->value == "a");
		CHECK(c.moves == 1);

		P f(c, "f");
		f = a;
		CHECK(f->value == "a");
		CHECK(c.copy_assigns == 1);

		P g(c, "g");
		g = std::move(f);
		CHECK(g.object().value == "a");
		CHECK(c.move_assigns == 1);
	}

	// a, b (moved-from), d, e, f (moved-from), g
	CHECK(c.destructions == 6);
}
