#include <tk/cli.hpp>

#include "../test_common.hpp"

#include <array>
#include <cstddef>
#include <expected>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tk::cli
{

namespace
{

Parser makeParser()
{
	Parser parser("prog", "Test program");

	parser.addFlag("help", 'h', "Display help information");
	parser.addOption({
		.name = "profile",
		.short_name = 'p',
		.type = Parser::ValueType::String,
		.description = "Profile name",
		.default_value = "default",
		.group = {},
	});
	parser.addOption({
		.name = "count",
		.short_name = 0,
		.type = Parser::ValueType::Integer,
		.description = "How many",
		.default_value = std::nullopt,
		.group = {},
	});
	parser.addOption({
		.name = "scale",
		.short_name = 0,
		.type = Parser::ValueType::Float,
		.description = "How much",
		.default_value = "1.5",
		.group = "Tuning",
	});
	parser.addFlag("gfx.debug", 0, "Enable debugging", "Graphics");
	parser.addFlag("verbose", 'v', "Verbose output");

	return parser;
}

template<size_t N>
std::expected<Args, std::string> parseArgs(const Parser &parser, const std::array<std::string_view, N> &args)
{
	return parser.parse(std::span<const std::string_view>(args));
}

} // namespace

TEST_CASE("CLI parser handles defaults", "[tk::cli]")
{
	Parser parser = makeParser();

	auto result = parser.parse(std::span<const std::string_view>());
	REQUIRE(result.has_value());

	const Args &args = *result;
	CHECK(args.entries().empty());
	CHECK(args.positional().empty());

	CHECK(!args.has("profile"));
	CHECK(args.value("profile") == "default");
	CHECK(!args.value("count").has_value());
	CHECK(!args.integer("count").has_value());
	CHECK(args.number("scale") == 1.5);
	CHECK(args.flag("help") == false);
	CHECK(args.value("help") == "false");

	// Unknown names are not errors on access
	CHECK(!args.value("unknown").has_value());
	CHECK(args.flag("unknown") == false);
}

TEST_CASE("CLI parser handles all syntax forms", "[tk::cli]")
{
	Parser parser = makeParser();

	auto result = parseArgs(parser,
		std::to_array<std::string_view>({ "first", "-h", "--profile=dev", "--count", "-42", "-p", "other", "--scale=2e3",
			"--gfx.debug=false", "-", "--verbose=true", "--", "--count", "-v" }));
	REQUIRE(result.has_value());

	const Args &args = *result;
	CHECK(args.flag("help"));
	CHECK(args.has("help"));
	// The last occurrence wins
	CHECK(args.value("profile") == "other");
	CHECK(args.integer("count") == -42);
	CHECK(args.number("scale") == 2000.0);
	CHECK(args.has("gfx.debug"));
	CHECK(args.flag("gfx.debug") == false);
	CHECK(args.flag("verbose"));

	REQUIRE(args.entries().size() == 7);
	CHECK(args.entries()[0].name == "help");
	CHECK(args.entries()[0].value == "true");
	CHECK(args.entries()[1].name == "profile");
	CHECK(args.entries()[1].value == "dev");
	CHECK(args.entries()[3].name == "profile");
	CHECK(args.entries()[3].value == "other");

	CHECK(args.positional() == std::vector<std::string> { "first", "-", "--count", "-v" });

	// Negative numbers and a single `-` are values, not options
	result = parseArgs(parser, std::to_array<std::string_view>({ "--scale", "-.5", "--profile", "-" }));
	REQUIRE(result.has_value());
	CHECK(result->number("scale") == -0.5);
	CHECK(result->value("profile") == "-");

	// Values looking like options can be passed inline
	result = parseArgs(parser, std::to_array<std::string_view>({ "--profile=--weird" }));
	REQUIRE(result.has_value());
	CHECK(result->value("profile") == "--weird");

	// Empty value is fine for strings
	result = parseArgs(parser, std::to_array<std::string_view>({ "--profile=" }));
	REQUIRE(result.has_value());
	CHECK(result->value("profile") == "");

	// `argc/argv` form skips the program name
	const char *const argv[] = { "prog", "-v", "pos" };
	result = parser.parse(3, argv);
	REQUIRE(result.has_value());
	CHECK(result->flag("verbose"));
	CHECK(result->positional() == std::vector<std::string> { "pos" });
}

TEST_CASE("CLI parser reports errors", "[tk::cli]")
{
	Parser parser = makeParser();

	auto check_error = [&](std::initializer_list<std::string_view> args, std::string_view expected) {
		std::vector<std::string_view> vec(args);
		auto result = parser.parse(vec);
		REQUIRE(!result.has_value());
		CHECK(result.error() == expected);
	};

	check_error({ "--unknown" }, "unknown option '--unknown'");
	check_error({ "-x" }, "unknown option '-x'");
	check_error({ "-hv" }, "unknown option '-hv'");
	check_error({ "--help=yes" }, "option '--help' accepts only 'true' or 'false' value");
	check_error({ "--profile" }, "option '--profile' requires a value");
	check_error({ "-p" }, "option '-p' requires a value");
	check_error({ "--profile", "--verbose" },
		"option '--profile' requires a value, use '--profile=<value>' if it starts with '-'");
	check_error({ "-p", "-v" }, "option '-p' requires a value, use '--profile=<value>' if it starts with '-'");
	check_error({ "--count", "--" }, "option '--count' requires a value, use '--count=<value>' if it starts with '-'");
	check_error({ "--count=12abc" }, "option '--count' requires an integer value, got '12abc'");
	check_error({ "--count", "1.5" }, "option '--count' requires an integer value, got '1.5'");
	check_error({ "--count=99999999999999999999" },
		"option '--count' requires an integer value, got '99999999999999999999'");
	check_error({ "--scale=fast" }, "option '--scale' requires a number value, got 'fast'");
	check_error({ "--scale=" }, "option '--scale' requires a number value, got ''");
	check_error({ "--scale=inf" }, "option '--scale' requires a number value, got 'inf'");
	check_error({ "--scale", "nan" }, "option '--scale' requires a number value, got 'nan'");
}

TEST_CASE("CLI parser formats help text", "[tk::cli]")
{
	Parser parser = makeParser();
	parser.addOption({
		.name = "very-long-option-name-that-does-not-fit",
		.short_name = 0,
		.type = Parser::ValueType::String,
		.description = "Long one",
		.default_value = std::nullopt,
		.group = "Tuning",
	});

	CHECK(parser.helpText() == R"(Test program

Usage: prog [options]

Options:
  -h, --help                                Display help information
  -p, --profile <string>                    Profile name [default: default]
      --count <int>                         How many
  -v, --verbose                             Verbose output

Tuning:
      --scale <number>                      How much [default: 1.5]
      --very-long-option-name-that-does-not-fit <string>
                                            Long one

Graphics:
      --gfx.debug                           Enable debugging
)");
}

} // namespace tk::cli
