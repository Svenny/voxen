#include <tk/json.hpp>

#include "../test_common.hpp"

#include <tk/parse_error.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace tk::json
{

namespace
{

Value parseOk(std::string_view text, Dialect dialect)
{
	auto result = parse(text, dialect);
	if (!result) {
		FAIL("Parse failed: " << result.error().toString() << "\nDocument:\n" << text);
	}
	return std::move(*result);
}

ParseError parseFail(std::string_view text, Dialect dialect)
{
	auto result = parse(text, dialect);
	if (result) {
		FAIL("Parse succeeded but should have failed, document:\n" << text);
	}
	return result.error();
}

} // namespace

// Characters would silently become booleans otherwise
static_assert(!std::is_constructible_v<Value, char>);

TEST_CASE("JSON parses all value types", "[tk::json]")
{
	for (Dialect dialect : { Dialect::Json, Dialect::Json5 }) {
		Value v = parseOk(R"(
{
	"null": null,
	"true": true,
	"false": false,
	"int": -42,
	"zero": 0,
	"float": 3.25e2,
	"neg_float": -0.5,
	"exp": 1E-3,
	"big": 12345678901234567890,
	"max": 9223372036854775807,
	"min": -9223372036854775808,
	"string": "hello",
	"array": [1, "two", [3], {}],
	"empty_array": [],
	"object": { "nested": { "deep": true } },
	"": "empty key"
}
)",
			dialect);

		const Object *o = v.get<Object>();
		REQUIRE(o != nullptr);
		CHECK(o->size() == 16);
		CHECK(o->begin()->first == "null");

		CHECK(o->find("null")->isNull());
		CHECK(*o->find("true") == Value(true));
		CHECK(*o->find("false") == Value(false));
		CHECK(*o->find("int") == Value(-42));
		CHECK(*o->find("zero") == Value(0));
		CHECK(*o->find("float") == Value(325.0));
		CHECK(*o->find("neg_float") == Value(-0.5));
		CHECK(*o->find("exp") == Value(0.001));
		// Doesn't fit into `int64_t`, becomes floating-point
		CHECK(*o->find("big") == Value(12345678901234567890.0));
		CHECK(*o->find("max") == Value(INT64_MAX));
		CHECK(*o->find("min") == Value(INT64_MIN));
		CHECK(*o->find("string") == Value("hello"));
		CHECK(*o->find("array") == Value(Array { 1, "two", Array { 3 }, Object {} }));
		CHECK(o->find("empty_array")->get<Array>()->empty());
		CHECK(*o->find("object") == Value(Object { { "nested", Object { { "deep", true } } } }));
		CHECK(*o->find("") == Value("empty key"));

		// Negative zero can't be an integer
		CHECK(o->find("zero")->type() == Value::Type::Integer);
		CHECK(parseOk("-0", dialect).type() == Value::Type::Float);
		CHECK(std::signbit(*parseOk("-0", dialect).get<double>()));
		CHECK(std::signbit(*parseOk("-0.0", dialect).get<double>()));

		// Underflow flushes to zero, keeping the sign
		CHECK(parseOk("1e-400", dialect) == Value(0.0));
		CHECK(std::signbit(*parseOk("-1e-400", dialect).get<double>()));
		CHECK(parseOk("0.000001e-320", dialect) == Value(0.0));
		CHECK(parseOk("4e-320", dialect) == Value(4e-320));

		CHECK(o->find("int")->number() == -42.0);
		CHECK(o->find("float")->number() == 325.0);
		CHECK(!o->find("string")->number().has_value());

		// Top-level scalars are fine
		CHECK(parseOk("42", dialect) == Value(42));
		CHECK(parseOk(" \"str\" ", dialect) == Value("str"));
		CHECK(parseOk("null", dialect).isNull());
		CHECK(parseOk("\xEF\xBB\xBF[]", dialect) == Value(Array {}));
	}
}

TEST_CASE("JSON parses string escapes", "[tk::json]")
{
	for (Dialect dialect : { Dialect::Json, Dialect::Json5 }) {
		Value v = parseOk(R"(["\"\\\/\b\f\n\r\t", "\u0041\u00e9\u20AC", "\uD83D\uDE00", "Привет", "del)"
								"\x7F"
								R"("])",
			dialect);

		const Array &a = *v.get<Array>();
		CHECK(a[0] == Value("\"\\/\b\f\n\r\t"));
		CHECK(a[1] == Value("A\xC3\xA9\xE2\x82\xAC"));
		CHECK(a[2] == Value("\xF0\x9F\x98\x80"));
		CHECK(a[3] == Value("Привет"));
		CHECK(a[4] == Value("del\x7F"));
	}
}

TEST_CASE("JSON5 parses its extensions", "[tk::json]")
{
	Value v = parseOk(R"(
// JSON5 example from https://json5.org
{
  // comments
  unquoted: 'and you can quote me on that',
  singleQuotes: 'I can use "double quotes" here',
  lineBreaks: "Look, Mom! \
No \\n's!",
  hexadecimal: 0xdecaf,
  leadingDecimalPoint: .8675309, andTrailing: 8675309.,
  positiveSign: +1,
  trailingComma: 'in objects', andIn: ['arrays',],
  "backwardsCompatible": "with JSON",
  /* block
     comment */ $dollar_id9: 1,
  _under: 2,
  ключ: 3,
  \u0061bc: 4,
  infinities: [Infinity, -Infinity, +Infinity],
  nan: [NaN, -NaN],
  hexes: [0X1F, -0x10, 0xFFFFFFFFFFFFFFFF, -0x8000000000000000],
  escapes: '\'\v\0\x41\a\é',
  rawControl: '	tab',
  continuations: 'a\)"
							"\nb\\\r\nc\\\rd\\\xE2\x80\xA8"
							R"(e',
}
)",
		Dialect::Json5);

	const Object &o = *v.get<Object>();
	CHECK(*o.find("unquoted") == Value("and you can quote me on that"));
	CHECK(*o.find("singleQuotes") == Value("I can use \"double quotes\" here"));
	CHECK(*o.find("lineBreaks") == Value("Look, Mom! No \\n's!"));
	CHECK(*o.find("hexadecimal") == Value(0xdecaf));
	CHECK(*o.find("leadingDecimalPoint") == Value(0.8675309));
	CHECK(*o.find("andTrailing") == Value(8675309.0));
	CHECK(*o.find("positiveSign") == Value(1));
	CHECK(*o.find("andIn") == Value(Array { "arrays" }));
	CHECK(*o.find("backwardsCompatible") == Value("with JSON"));
	CHECK(*o.find("$dollar_id9") == Value(1));
	CHECK(*o.find("_under") == Value(2));
	CHECK(*o.find("ключ") == Value(3));
	CHECK(*o.find("abc") == Value(4));

	constexpr double INF = std::numeric_limits<double>::infinity();
	CHECK(*o.find("infinities") == Value(Array { INF, -INF, INF }));

	const Array &nans = *o.find("nan")->get<Array>();
	CHECK(std::isnan(*nans[0].get<double>()));
	CHECK(std::isnan(*nans[1].get<double>()));

	CHECK(*o.find("hexes") == Value(Array { 31, -16, 18446744073709551615.0, INT64_MIN }));

	// Large hex numbers are correctly rounded
	CHECK(parseOk("0x4a525613366cb2cc3a22d556", Dialect::Json5) == Value(2.3001429121915306e+28));
	CHECK(parseOk("-0x8000000000000001", Dialect::Json5) == Value(-9223372036854775809.0));
	CHECK(std::signbit(*parseOk("-0x0", Dialect::Json5).get<double>()));
	CHECK(*o.find("escapes") == Value(std::string("'\v\0Aa\xC3\xA9", 7)));
	CHECK(*o.find("continuations") == Value("abcde"));
	CHECK(*o.find("rawControl") == Value("\ttab"));

	// Additional whitespace
	CHECK(parseOk("\v\f\xC2\xA0\xE2\x80\xA8\xE3\x80\x80[1]\xEF\xBB\xBF", Dialect::Json5) == Value(Array { 1 }));
	CHECK(parseOk("[1] // comment at the end", Dialect::Json5) == Value(Array { 1 }));
	CHECK(parseOk("/**/[/**/1/**/]/**/", Dialect::Json5) == Value(Array { 1 }));
}

TEST_CASE("JSON rejects invalid documents", "[tk::json]")
{
	// Invalid in both dialects
	const char *const INVALID[] = {
		"",
		"   ",
		"[",
		"]",
		"{",
		"[1 2]",
		"[1,,2]",
		"[,]",
		"{\"a\" 1}",
		"{\"a\": }",
		"{\"a\": 1 \"b\": 2}",
		"{\"a\": 1,, \"b\": 2}",
		"{1: 2}",
		"{\"a\": 1, \"a\": 2}",
		"[1] [2]",
		"nul",
		"True",
		"01",
		"-01",
		"1e",
		"1e+",
		"--1",
		"-",
		"1e400",
		"\"unterminated",
		"\"new\nline\"",
		"\"bad \\u12\"",
		"\"lone \\uD800\"",
		"\"lone \\uDC00\"",
		"\"pair \\uD800\\u0041\"",
		"\"\xC0\x80\"",
		"\"\xED\xA0\x80\"",
		"\"\xFF\"",
		"[1] /* unterminated",
	};

	for (Dialect dialect : { Dialect::Json, Dialect::Json5 }) {
		for (const char *text : INVALID) {
			INFO("Document: " << text);
			CHECK(!parse(text, dialect).has_value());
		}
	}

	// Valid JSON5 but invalid JSON
	const char *const JSON5_ONLY[] = {
		"[1,]",
		"{\"a\": 1,}",
		"{a: 1}",
		"'single'",
		"// comment\n1",
		"/* comment */ 1",
		"0x10",
		".5",
		"5.",
		"1.e5",
		"+1",
		"Infinity",
		"-Infinity",
		"NaN",
		"\"\\'\"",
		"\"\\q\"",
		"\"\\v\"",
		"\"\\x41\"",
		"\"line \\\ncontinuation\"",
		"\"raw\ttab\"",
		"\v1",
		("\xC2\xA0"
		 "1"),
	};

	for (const char *text : JSON5_ONLY) {
		INFO("Document: " << text);
		CHECK(!parse(text, Dialect::Json).has_value());
		CHECK(parse(text, Dialect::Json5).has_value());
	}

	// Invalid JSON5 specifically
	const char *const INVALID_JSON5[] = {
		"[1,,]",
		"{a: 1,,}",
		"{1a: 1}",
		"{a-b: 1}",
		"{'a': 1, a: 2}",
		"0x",
		"0xG",
		".",
		"+",
		"+.e1",
		"'\\1'",
		"'\\01'",
		"'\\xG0'",
		"{\\x61: 1}",
		"Infinityx",
	};

	for (const char *text : INVALID_JSON5) {
		INFO("Document: " << text);
		CHECK(!parse(text, Dialect::Json5).has_value());
	}
}

TEST_CASE("JSON finds duplicate keys in large objects", "[tk::json]")
{
	// Large enough to switch from linear search to hashing
	auto make_object = [](int count, std::string_view extra) {
		std::string text = "{";
		for (int i = 0; i < count; i++) {
			text += "\"k" + std::to_string(i) + "\": " + std::to_string(i) + ", ";
		}
		text += extra;
		text += "}";
		return text;
	};

	for (Dialect dialect : { Dialect::Json, Dialect::Json5 }) {
		auto result = parse(make_object(1000, "\"last\": 0"), dialect);
		REQUIRE(result.has_value());
		CHECK(result->get<Object>()->size() == 1001);
		CHECK(*result->get<Object>()->find("k999") == Value(999));

		// Duplicate of a key added before and after switching to hashing
		CHECK(!parse(make_object(1000, "\"k5\": 0"), dialect).has_value());
		CHECK(!parse(make_object(1000, "\"k500\": 0"), dialect).has_value());
		CHECK(!parse(make_object(10, "\"k5\": 0"), dialect).has_value());
	}
}

TEST_CASE("JSON reports error locations", "[tk::json]")
{
	ParseError e = parseFail("{\n  \"a\": 1,\n  \"b\": tru\n}", Dialect::Json);
	CHECK(e.line == 3);
	CHECK(e.column == 8);
	CHECK(e.toString().starts_with("3:8: "));

	e = parseFail("{\n  \"a\": 1,\n  \"a\": 2\n}", Dialect::Json);
	CHECK(e.line == 3);
	CHECK(e.column == 3);

	e = parseFail("[\"ключ\", 01]", Dialect::Json);
	CHECK(e.line == 1);
	CHECK(e.column == 10);

	e = parseFail("// comment\n{ a: 1 b: 2 }", Dialect::Json5);
	CHECK(e.line == 2);
	CHECK(e.column == 8);
}

TEST_CASE("JSON limits nesting depth", "[tk::json]")
{
	for (Dialect dialect : { Dialect::Json, Dialect::Json5 }) {
		std::string deep = std::string(128, '[') + std::string(128, ']');
		CHECK(parse(deep, dialect).has_value());
		CHECK(!parse("[" + deep + "]", dialect).has_value());

		std::string too_deep = std::string(100'000, '[') + std::string(100'000, ']');
		CHECK(!parse(too_deep, dialect).has_value());

		std::string deep_objects;
		for (int i = 0; i < 100'000; i++) {
			deep_objects += "{\"a\":";
		}
		CHECK(!parse(deep_objects, dialect).has_value());
	}
}

} // namespace tk::json
