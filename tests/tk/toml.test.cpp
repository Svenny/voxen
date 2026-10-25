#include <tk/toml.hpp>

#include "../test_common.hpp"

#include <tk/parse_error.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace tk::toml
{

namespace
{

Table parseOk(std::string_view text)
{
	auto result = parse(text);
	if (!result) {
		FAIL("Parse failed: " << result.error().toString() << "\nDocument:\n" << text);
	}
	return std::move(*result);
}

ParseError parseFail(std::string_view text)
{
	auto result = parse(text);
	if (result) {
		FAIL("Parse succeeded but should have failed, document:\n" << text);
	}
	return result.error();
}

template<typename T>
const T &get(const Table &table, std::string_view key)
{
	const Value *value = table.find(key);
	REQUIRE(value != nullptr);
	const T *typed = value->get<T>();
	REQUIRE(typed != nullptr);
	return *typed;
}

} // namespace

// Characters would silently become booleans otherwise
static_assert(!std::is_constructible_v<Value, char>);

TEST_CASE("TOML parses basic key-value pairs", "[tk::toml]")
{
	Table t = parseOk(R"(
# Comment line
str = "hello"   # trailing comment
int = 42
neg = -17
flt = 3.25
yes = true
no = false
bare_key-1 = 1
1234 = "digits are a bare key"
"quoted key" = 2
'literal key' = 3
"" = "empty key"
)");

	CHECK(t.size() == 11);
	CHECK(get<std::string>(t, "str") == "hello");
	CHECK(get<int64_t>(t, "int") == 42);
	CHECK(get<int64_t>(t, "neg") == -17);
	CHECK(get<double>(t, "flt") == 3.25);
	CHECK(get<bool>(t, "yes") == true);
	CHECK(get<bool>(t, "no") == false);
	CHECK(get<int64_t>(t, "bare_key-1") == 1);
	CHECK(get<std::string>(t, "1234") == "digits are a bare key");
	CHECK(get<int64_t>(t, "quoted key") == 2);
	CHECK(get<int64_t>(t, "literal key") == 3);
	CHECK(get<std::string>(t, "") == "empty key");

	// Insertion order is kept
	CHECK(t.begin()->first == "str");

	CHECK(parseOk("").empty());
	CHECK(parseOk("\n\n# just comments\r\n  \t\n").empty());
	CHECK(parseOk("a=1").size() == 1);
	CHECK(parseOk("\xEF\xBB\xBF"
					  "a = 1\r\nb = 2\r\n")
				.size()
		== 2);
}

TEST_CASE("TOML parses strings", "[tk::toml]")
{
	SECTION("Basic strings with escapes")
	{
		Table t = parseOk(R"(
a = "tab\there"
b = "quote\" backslash\\ slash/"
c = "\b\f\n\r"
d = "\u00E9\U0001F600"
e = "Привет, мир"
)");
		CHECK(get<std::string>(t, "a") == "tab\there");
		CHECK(get<std::string>(t, "b") == "quote\" backslash\\ slash/");
		CHECK(get<std::string>(t, "c") == "\b\f\n\r");
		CHECK(get<std::string>(t, "d") == "\xC3\xA9\xF0\x9F\x98\x80");
		CHECK(get<std::string>(t, "e") == "Привет, мир");
	}

	SECTION("Literal strings")
	{
		Table t = parseOk(R"(
winpath = 'C:\Users\nodejs\templates'
quoted = 'Tom "Dubs" Preston-Werner'
regex = '<\i\c*\s*>'
)");
		CHECK(get<std::string>(t, "winpath") == R"(C:\Users\nodejs\templates)");
		CHECK(get<std::string>(t, "quoted") == R"(Tom "Dubs" Preston-Werner)");
		CHECK(get<std::string>(t, "regex") == R"(<\i\c*\s*>)");
	}

	SECTION("Multi-line basic strings")
	{
		Table t = parseOk(
			"a = \"\"\"\nRoses are red\nViolets are blue\"\"\"\n"
			"b = \"\"\"\\\n  The quick brown \\\n\n\n  fox jumps over \\\n    the lazy dog.\\\n  \"\"\"\n"
			"c = \"\"\"Here are two quotation marks: \"\". Simple enough.\"\"\"\n"
			"d = \"\"\"Here are three quotation marks: \"\"\\\".\"\"\"\n"
			"e = \"\"\"\"This,\" she said, \"is just a pointless statement.\"\"\"\"\n"
			"f = \"\"\"windows\r\nnewline\"\"\"\n"
			"g = \"\"\"trailing\\  \t\r\n  ws\"\"\"\n");
		CHECK(get<std::string>(t, "a") == "Roses are red\nViolets are blue");
		CHECK(get<std::string>(t, "b") == "The quick brown fox jumps over the lazy dog.");
		CHECK(get<std::string>(t, "c") == "Here are two quotation marks: \"\". Simple enough.");
		CHECK(get<std::string>(t, "d") == "Here are three quotation marks: \"\"\".");
		CHECK(get<std::string>(t, "e") == "\"This,\" she said, \"is just a pointless statement.\"");
		CHECK(get<std::string>(t, "f") == "windows\nnewline");
		CHECK(get<std::string>(t, "g") == "trailingws");
	}

	SECTION("Multi-line literal strings")
	{
		Table t = parseOk(
			"regex2 = '''I [dw]on't need \\d{2} apples'''\n"
			"lines = '''\nThe first newline is\ntrimmed in raw strings.\n   All other whitespace\n"
			"   is preserved.\n'''\n"
			"quot15 = '''Here are fifteen quotation marks: \"\"\"\"\"\"\"\"\"\"\"\"\"\"\"'''\n"
			"str = ''''That,' she said, 'is still pointless.''''\n");
		CHECK(get<std::string>(t, "regex2") == "I [dw]on't need \\d{2} apples");
		CHECK(get<std::string>(t, "lines")
			== "The first newline is\ntrimmed in raw strings.\n   All other whitespace\n   is preserved.\n");
		CHECK(get<std::string>(t, "quot15") == "Here are fifteen quotation marks: \"\"\"\"\"\"\"\"\"\"\"\"\"\"\"");
		CHECK(get<std::string>(t, "str") == "'That,' she said, 'is still pointless.'");
	}
}

TEST_CASE("TOML parses numbers", "[tk::toml]")
{
	Table t = parseOk(R"(
int1 = +99
int2 = 0
int3 = -0
int4 = 1_000_000
int5 = 5_349_221
hex1 = 0xDEADBEEF
hex2 = 0xdead_beef
oct1 = 0o01234567
oct2 = 0o755
bin1 = 0b11010110
max = 9223372036854775807
min = -9223372036854775808
hexmax = 0x7FFFFFFFFFFFFFFF

flt1 = +1.0
flt2 = 3.1415
flt3 = -0.01
flt4 = 5e+22
flt5 = 1e06
flt6 = -2E-2
flt7 = 6.626e-34
flt8 = 224_617.445_991_228
flt9 = 0e0
tiny1 = 1e-400
tiny2 = -100e-400
tiny3 = 0.000_001e-320
tiny4 = 4e-320
sf1 = inf
sf2 = +inf
sf3 = -inf
sf4 = nan
sf5 = +nan
sf6 = -nan
)");

	CHECK(get<int64_t>(t, "int1") == 99);
	CHECK(get<int64_t>(t, "int2") == 0);
	CHECK(get<int64_t>(t, "int3") == 0);
	CHECK(get<int64_t>(t, "int4") == 1'000'000);
	CHECK(get<int64_t>(t, "int5") == 5'349'221);
	CHECK(get<int64_t>(t, "hex1") == 0xDEADBEEF);
	CHECK(get<int64_t>(t, "hex2") == 0xDEADBEEF);
	CHECK(get<int64_t>(t, "oct1") == 01234567);
	CHECK(get<int64_t>(t, "oct2") == 0755);
	CHECK(get<int64_t>(t, "bin1") == 0b11010110);
	CHECK(get<int64_t>(t, "max") == INT64_MAX);
	CHECK(get<int64_t>(t, "min") == INT64_MIN);
	CHECK(get<int64_t>(t, "hexmax") == INT64_MAX);

	CHECK(get<double>(t, "flt1") == 1.0);
	CHECK(get<double>(t, "flt2") == 3.1415);
	CHECK(get<double>(t, "flt3") == -0.01);
	CHECK(get<double>(t, "flt4") == 5e22);
	CHECK(get<double>(t, "flt5") == 1e6);
	CHECK(get<double>(t, "flt6") == -2e-2);
	CHECK(get<double>(t, "flt7") == 6.626e-34);
	CHECK(get<double>(t, "flt8") == 224617.445991228);
	CHECK(get<double>(t, "flt9") == 0.0);
	// Underflow flushes to zero, keeping the sign
	CHECK(get<double>(t, "tiny1") == 0.0);
	CHECK(!std::signbit(get<double>(t, "tiny1")));
	CHECK(get<double>(t, "tiny2") == 0.0);
	CHECK(std::signbit(get<double>(t, "tiny2")));
	CHECK(get<double>(t, "tiny3") == 0.0);
	// Subnormals are fine
	CHECK(get<double>(t, "tiny4") == 4e-320);
	CHECK(get<double>(t, "sf1") == std::numeric_limits<double>::infinity());
	CHECK(get<double>(t, "sf2") == std::numeric_limits<double>::infinity());
	CHECK(get<double>(t, "sf3") == -std::numeric_limits<double>::infinity());
	CHECK(std::isnan(get<double>(t, "sf4")));
	CHECK(std::isnan(get<double>(t, "sf5")));
	CHECK(std::isnan(get<double>(t, "sf6")));
}

TEST_CASE("TOML parses dates and times", "[tk::toml]")
{
	Table t = parseOk(R"(
odt1 = 1979-05-27T07:32:00Z
odt2 = 1979-05-27T00:32:00-07:00
odt3 = 1979-05-27T00:32:00.999999+05:30
odt4 = 1979-05-27 07:32:00z
ldt1 = 1979-05-27T07:32:00
ldt2 = 1979-05-27t00:32:00.123456789123
ld1 = 1979-05-27
lt1 = 07:32:00
lt2 = 00:32:00.5
leap = 2000-02-29
)");

	CHECK(get<OffsetDateTime>(t, "odt1") == OffsetDateTime { { 1979, 5, 27 }, { 7, 32, 0, 0 }, 0 });
	CHECK(get<OffsetDateTime>(t, "odt2") == OffsetDateTime { { 1979, 5, 27 }, { 0, 32, 0, 0 }, -420 });
	CHECK(get<OffsetDateTime>(t, "odt3") == OffsetDateTime { { 1979, 5, 27 }, { 0, 32, 0, 999'999'000 }, 330 });
	CHECK(get<OffsetDateTime>(t, "odt4") == OffsetDateTime { { 1979, 5, 27 }, { 7, 32, 0, 0 }, 0 });
	CHECK(get<LocalDateTime>(t, "ldt1") == LocalDateTime { { 1979, 5, 27 }, { 7, 32, 0, 0 } });
	CHECK(get<LocalDateTime>(t, "ldt2") == LocalDateTime { { 1979, 5, 27 }, { 0, 32, 0, 123'456'789 } });
	CHECK(get<LocalDate>(t, "ld1") == LocalDate { 1979, 5, 27 });
	CHECK(get<LocalTime>(t, "lt1") == LocalTime { 7, 32, 0, 0 });
	CHECK(get<LocalTime>(t, "lt2") == LocalTime { 0, 32, 0, 500'000'000 });
	CHECK(get<LocalDate>(t, "leap") == LocalDate { 2000, 2, 29 });
}

TEST_CASE("TOML parses arrays", "[tk::toml]")
{
	Table t = parseOk(R"(
integers = [ 1, 2, 3 ]
colors = [ "red", "yellow", "green" ]
nested_arrays_of_ints = [ [ 1, 2 ], [3, 4, 5] ]
nested_mixed_array = [ [ 1, 2 ], ["a", "b", "c"] ]
numbers = [ 0.1, 0.2, 0.5, 1, 2, 5 ]
contributors = [
  "Foo Bar <foo@example.com>", # comment
  { name = "Baz Qux", email = "bazqux@example.com", url = "https://example.com/bazqux" }
]
integers2 = [
  1, 2, 3
]
integers3 = [
  1,
  2, # this is ok
]
empty = []
empty2 = [ # comment
]
)");

	const Array &integers = get<Array>(t, "integers");
	REQUIRE(integers.size() == 3);
	CHECK(integers[2] == Value(3));

	CHECK(get<Array>(t, "colors") == Array { "red", "yellow", "green" });
	CHECK(get<Array>(t, "nested_arrays_of_ints") == Array { Array { 1, 2 }, Array { 3, 4, 5 } });
	CHECK(get<Array>(t, "nested_mixed_array") == Array { Array { 1, 2 }, Array { "a", "b", "c" } });
	CHECK(get<Array>(t, "numbers") == Array { 0.1, 0.2, 0.5, 1, 2, 5 });

	const Array &contributors = get<Array>(t, "contributors");
	REQUIRE(contributors.size() == 2);
	CHECK(contributors[0] == Value("Foo Bar <foo@example.com>"));
	const Table *contributor = contributors[1].get<Table>();
	REQUIRE(contributor != nullptr);
	CHECK(get<std::string>(*contributor, "name") == "Baz Qux");

	CHECK(get<Array>(t, "integers2").size() == 3);
	CHECK(get<Array>(t, "integers3").size() == 2);
	CHECK(get<Array>(t, "empty").empty());
	CHECK(get<Array>(t, "empty2").empty());
}

TEST_CASE("TOML parses tables", "[tk::toml]")
{
	SECTION("Headers and dotted keys")
	{
		Table t = parseOk(R"(
name = "Orange"
physical.color = "orange"
physical.shape = "round"
site."google.com" = true

[table-1]
key1 = "some string"
key2 = 123

[ dog . "tater.man" ]
type.name = "pug"

# [x] you
# [x.y] don't
# [x.y.z] need these
[x.y.z.w] # for this to work

[x] # defining a super-table afterward is ok

[fruit]
apple.color = "red"
apple.taste.sweet = true

[fruit.apple.texture]  # you can add sub-tables
smooth = true
)");

		const Table &physical = get<Table>(t, "physical");
		CHECK(get<std::string>(physical, "color") == "orange");
		CHECK(get<std::string>(physical, "shape") == "round");
		CHECK(get<bool>(get<Table>(t, "site"), "google.com") == true);

		const Table &table1 = get<Table>(t, "table-1");
		CHECK(get<int64_t>(table1, "key2") == 123);

		const Table &tater = get<Table>(get<Table>(t, "dog"), "tater.man");
		CHECK(get<std::string>(get<Table>(tater, "type"), "name") == "pug");

		const Table &w = get<Table>(get<Table>(get<Table>(get<Table>(t, "x"), "y"), "z"), "w");
		CHECK(w.empty());

		const Table &apple = get<Table>(get<Table>(t, "fruit"), "apple");
		CHECK(get<std::string>(apple, "color") == "red");
		CHECK(get<bool>(get<Table>(apple, "taste"), "sweet") == true);
		CHECK(get<bool>(get<Table>(apple, "texture"), "smooth") == true);
	}

	SECTION("Inline tables")
	{
		Table t = parseOk(R"(
name = { first = "Tom", last = "Preston-Werner" }
point = { x = 1, y = 2 }
animal = { type.name = "pug" }
empty = {}
nested = { a = { b = { c = 1 } } }
)");

		CHECK(get<Table>(t, "name") == Table { { "first", "Tom" }, { "last", "Preston-Werner" } });
		CHECK(get<Table>(t, "point") == Table { { "y", 2 }, { "x", 1 } });
		CHECK(get<std::string>(get<Table>(get<Table>(t, "animal"), "type"), "name") == "pug");
		CHECK(get<Table>(t, "empty").empty());
		CHECK(get<int64_t>(get<Table>(get<Table>(get<Table>(t, "nested"), "a"), "b"), "c") == 1);
	}

	SECTION("Arrays of tables")
	{
		Table t = parseOk(R"(
[[products]]
name = "Hammer"
sku = 738594937

[[products]]  # empty table within the array

[[products]]
name = "Nail"
sku = 284758393

color = "gray"

[[fruits]]
name = "apple"

[fruits.physical]  # subtable
color = "red"
shape = "round"

[[fruits.varieties]]  # nested array of tables
name = "red delicious"

[[fruits.varieties]]
name = "granny smith"

[[fruits]]
name = "banana"

[[fruits.varieties]]
name = "plantain"
)");

		const Array &products = get<Array>(t, "products");
		REQUIRE(products.size() == 3);
		CHECK(get<std::string>(*products[0].get<Table>(), "name") == "Hammer");
		CHECK(products[1].get<Table>()->empty());
		CHECK(get<std::string>(*products[2].get<Table>(), "color") == "gray");

		const Array &fruits = get<Array>(t, "fruits");
		REQUIRE(fruits.size() == 2);
		const Table &apple = *fruits[0].get<Table>();
		CHECK(get<std::string>(get<Table>(apple, "physical"), "shape") == "round");
		CHECK(get<Array>(apple, "varieties").size() == 2);
		const Table &banana = *fruits[1].get<Table>();
		CHECK(get<Array>(banana, "varieties").size() == 1);
	}
}

TEST_CASE("TOML rejects invalid documents", "[tk::toml]")
{
	const char *const INVALID[] = {
		// Syntax
		"key",
		"key =",
		"= 1",
		"key = # no value",
		"first = \"Tom\" last = \"Preston-Werner\"",
		"a = 1\rb = 2",
		"a.b.c",
		"a. = 1",
		".a = 1",
		"a = 1 2",
		"[table",
		"[]",
		"[[table]",
		"[[table] ]",
		"[a]b = 1",
		"a = \"\"\"key\"\"\" = 1",
		"\"\"\"key\"\"\" = 1",
		"a = 'x' = 1",
		// Strings
		"a = \"unterminated",
		"a = 'unterminated",
		"a = \"\"\"unterminated",
		"a = '''unterminated",
		"a = \"new\nline\"",
		"a = 'new\nline'",
		"a = \"bad \\x escape\"",
		"a = \"bad \\u12 escape\"",
		"a = \"surrogate \\uD800\"",
		"a = \"too big \\U00110000\"",
		"a = \"\"\"line ending \\ x\"\"\"",
		"a = \"tab\x01\"",
		"a = \"del\x7F\"",
		"a = '''six quotes''''''",
		"a = \"\"\"six quotes\"\"\"\"\"\"",
		"# control \x01 in comment",
		// Numbers
		"a = 01",
		"a = 00",
		"a = 0_1",
		"a = 1__000",
		"a = _1",
		"a = 1_",
		"a = +0x1",
		"a = 0x",
		"a = 0xG",
		"a = 0o8",
		"a = 0b2",
		"a = 0X1",
		"a = 9223372036854775808",
		"a = -9223372036854775809",
		"a = 0x8000000000000000",
		"a = 1.",
		"a = .5",
		"a = 1.e5",
		"a = 1e",
		"a = 1e_5",
		"a = 1.5_",
		"a = 01.5",
		"a = 1e400",
		"a = infinity",
		"a = NaN",
		"a = +-1",
		// Booleans
		"a = True",
		"a = truex",
		// Dates and times
		"a = 1979-13-01",
		"a = 1979-02-30",
		"a = 1979-02-29",
		"a = 1979-00-10",
		"a = 1979-05-27T24:00:00",
		"a = 1979-05-27T23:60:00",
		"a = 1979-05-27T23:59:61",
		"a = 1979-05-27T07:32",
		"a = 1979-05-27T07:32:00.",
		"a = 1979-05-27T07:32:00+25:00",
		"a = 1979-05-27T07:32:00+05",
		"a = 1979-5-27",
		"a = 07:32",
		// Arrays and inline tables
		"a = [1 2]",
		"a = [,]",
		"a = [1,,2]",
		"a = [1",
		"a = { b = 1, }",
		"a = { b = 1 c = 2 }",
		"a = { b = 1,\n c = 2 }",
		"a = { b = 1, b = 2 }",
		"a = {",
		// Redefinitions
		"a = 1\na = 2",
		"a = 1\n\"a\" = 2",
		"a.b = 1\na.b = 2",
		"a.b = 1\na = 2",
		"a = 1\na.b = 2",
		"[a]\n[a]",
		"[a]\nb = 1\n[a.b]",
		"[a.b]\n[a]\nb = 1",
		"[a]\nb.c = 1\n[a.b]",
		"[a.b.c]\n[a]\nb.c.d = 1",
		"a = {}\n[a]",
		"a = { b = 1 }\n[a.c]",
		"a = { b = { c = 1 } }\n[a.b]",
		"a = { b.c = 1 }\na.b.d = 2",
		"a = []\n[[a]]",
		"a = [{}]\n[[a]]",
		"a = [{}]\n[a.b]",
		"[[a]]\n[a]",
		"[a]\n[[a]]",
		"a = 1\n[a.b]",
		"a = 1\n[[a.b]]",
		"[a.b]\nc = 1\n[a]\nb.d = 2",
		// Encoding
		"a = \"\xC0\x80\"",
		"a = \"\xED\xA0\x80\"",
		"a = \"\xF4\x90\x80\x80\"",
		"a = \"\xE2\x82\"",
		"a = \"\x80\"",
	};

	for (const char *text : INVALID) {
		INFO("Document: " << text);
		CHECK(!parse(text).has_value());
	}
}

TEST_CASE("TOML reports error locations", "[tk::toml]")
{
	ParseError e = parseFail("a = 1\nb = \"ok\"\nc = 01\n");
	CHECK(e.line == 3);
	CHECK(e.column == 5);
	CHECK(e.toString().starts_with("3:5: "));

	e = parseFail("[a]\nx = 1\n[b]\n[a]\n");
	CHECK(e.line == 4);
	CHECK(e.column == 2);

	// Column counts code points, not bytes
	e = parseFail("\"ключ\" = $");
	CHECK(e.line == 1);
	CHECK(e.column == 10);

	e = parseFail("a = \"unterminated\nb = 1");
	CHECK(e.line == 1);
	CHECK(e.column == 5);

	e = parseFail("a = [\n  1,\n  2\n  3\n]");
	CHECK(e.line == 4);
	CHECK(e.column == 3);
}

TEST_CASE("TOML limits nesting depth", "[tk::toml]")
{
	std::string deep = "a = " + std::string(100, '[') + std::string(100, ']');
	CHECK(parse(deep).has_value());

	std::string too_deep = "a = " + std::string(100'000, '[') + std::string(100'000, ']');
	CHECK(!parse(too_deep).has_value());

	std::string deep_tables = "a = ";
	for (int i = 0; i < 100'000; i++) {
		deep_tables += "{ b = ";
	}
	CHECK(!parse(deep_tables).has_value());

	// Tables nest with dotted keys and headers too
	auto dotted = [](int count) {
		std::string key = "a";
		for (int i = 1; i < count; i++) {
			key += ".a";
		}
		return key;
	};

	CHECK(parse(dotted(100) + " = 1").has_value());
	CHECK(!parse(dotted(1'000'000) + " = 1").has_value());
	CHECK(parse("[" + dotted(100) + "]").has_value());
	CHECK(!parse("[" + dotted(1'000'000) + "]").has_value());
	CHECK(!parse("[[" + dotted(1'000'000) + "]]").has_value());
	// Exactly at the limit
	CHECK(parse("[" + dotted(128) + "]").has_value());
	CHECK(!parse("[" + dotted(129) + "]").has_value());
	CHECK(parse(dotted(129) + " = 1").has_value());
	CHECK(!parse(dotted(130) + " = 1").has_value());

	// Levels sum up: header + dotted key + arrays/inline tables
	CHECK(parse("[" + dotted(60) + "]\n" + dotted(60) + " = [[1]]").has_value());
	CHECK(!parse("[" + dotted(100) + "]\n" + dotted(100) + " = 1").has_value());
	CHECK(!parse("[" + dotted(100) + "]\nb = " + std::string(100, '[') + std::string(100, ']')).has_value());
	CHECK(!parse("[[" + dotted(100) + "]]\n[" + dotted(100) + ".b]\n" + dotted(100) + " = 1").has_value());
	CHECK(!parse("a = { " + dotted(100) + " = " + std::string(100, '[') + std::string(100, ']') + " }").has_value());
}

TEST_CASE("TOML formats values", "[tk::toml]")
{
	CHECK(formatKey("bare_key-1") == "bare_key-1");
	CHECK(formatKey("with space") == "\"with space\"");
	CHECK(formatKey("") == "\"\"");
	CHECK(formatKey("a.b") == "\"a.b\"");
	CHECK(formatKey("ключ") == "\"ключ\"");

	CHECK(formatValue("plain") == "\"plain\"");
	CHECK(formatValue("q\"b\\t\tn\nc\x01"
							"d\x7F")
		== R"("q\"b\\t\tn\nc\u0001d\u007F")");
	CHECK(formatValue(42) == "42");
	CHECK(formatValue(int64_t(INT64_MIN)) == "-9223372036854775808");
	CHECK(formatValue(1.0) == "1.0");
	CHECK(formatValue(-0.0) == "-0.0");
	CHECK(formatValue(0.1) == "0.1");
	CHECK(formatValue(1e100) == "1e+100");
	CHECK(formatValue(std::numeric_limits<double>::infinity()) == "inf");
	CHECK(formatValue(-std::numeric_limits<double>::infinity()) == "-inf");
	CHECK(formatValue(std::numeric_limits<double>::quiet_NaN()) == "nan");
	CHECK(formatValue(true) == "true");
	CHECK(formatValue(Array { 1, "a", Array {} }) == "[1, \"a\", []]");
	CHECK(formatValue(Table {}) == "{}");
	CHECK(
		formatValue(Table { { "a", 1 }, { "b c", Table { { "d", false } } } }) == "{ a = 1, \"b c\" = { d = false } }");

	CHECK(formatValue(OffsetDateTime { { 1979, 5, 27 }, { 7, 32, 0, 0 }, 0 }) == "1979-05-27T07:32:00Z");
	CHECK(formatValue(OffsetDateTime { { 1979, 5, 27 }, { 0, 32, 0, 999'999'000 }, -420 })
		== "1979-05-27T00:32:00.999999-07:00");
	CHECK(formatValue(LocalDateTime { { 1, 1, 1 }, { 23, 59, 60, 1 } }) == "0001-01-01T23:59:60.000000001");
	CHECK(formatValue(LocalDate { 2024, 12, 31 }) == "2024-12-31");
	CHECK(formatValue(LocalTime { 7, 5, 3, 500'000'000 }) == "07:05:03.5");
}

TEST_CASE("TOML serializes documents", "[tk::toml]")
{
	SECTION("Section layout")
	{
		Table t {
			{ "title", "example" },
			{ "owner", Table { { "name", "Tom" } } },
			{ "a", Table { { "b", Table { { "c", 1 } } } } },
			{ "empty", Table {} },
			{ "points", Array { Table { { "x", 1 } }, Table {} } },
			{ "after", 2 },
		};

		CHECK(serialize(t) == R"(title = "example"
after = 2

[owner]
name = "Tom"

[a.b]
c = 1

[empty]

[[points]]
x = 1

[[points]]
)");
		CHECK(parseOk(serialize(t)) == t);
	}

	SECTION("Round trip")
	{
		const char *const DOCUMENTS[] = {
			R"(
str = "multi\nline \"quoted\" \u00E9"
lit = '''C:\path'''
numbers = [ 0x10, 1e-5, -inf, 1979-05-27T07:32:00Z, 07:32:00.25, 2000-01-01 ]
inline = { a.b = 1, c = [ { d = 2 } ] }

[[fruits]]
name = "apple"
[fruits.physical]
color = "red"
[[fruits.varieties]]
name = "red delicious"
[[fruits]]
name = "banana"

[x.y.z.w]
[x]
"weird key" = { "" = '' }
)",
			"a.b.c.d = []\n",
			"[[a.b]]\n[[a.b]]\nc = 1\n[[a.b.c2]]\n",
		};

		for (const char *document : DOCUMENTS) {
			INFO("Document:\n" << document);
			Table original = parseOk(document);
			std::string serialized = serialize(original);
			INFO("Serialized:\n" << serialized);
			CHECK(parseOk(serialized) == original);
			// Serialization must be stable
			CHECK(serialize(parseOk(serialized)) == serialized);
		}
	}
}

} // namespace tk::toml
