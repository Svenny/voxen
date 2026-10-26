#include <voxen/svc/config.hpp>

#include <tk/toml.hpp>

#include "../../test_common.hpp"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace voxen::svc
{

namespace
{

// Returns names of options whose values in the text don't match the scheme defaults
std::vector<std::string> findMismatches(const Config::Scheme &scheme, const tk::toml::Table &table)
{
	std::vector<std::string> mismatches;

	for (const Config::SchemeEntry &entry : scheme) {
		const tk::toml::Value *section = table.find(entry.section);
		const tk::toml::Table *section_table = section ? section->get<tk::toml::Table>() : nullptr;
		const tk::toml::Value *value = section_table ? section_table->find(entry.parameter_name) : nullptr;

		auto expected = std::visit([](const auto &v) { return tk::toml::Value(v); }, entry.default_value);
		if (!value || !(*value == expected)) {
			mismatches.push_back(entry.section + "." + entry.parameter_name);
		}
	}

	return mismatches;
}

} // namespace

TEST_CASE("'Config' default file text matches the scheme", "[voxen::svc::config]")
{
	SECTION("Main config scheme")
	{
		const Config::Scheme scheme = Config::mainConfigScheme();

		auto table = tk::toml::parse(Config::defaultFileText(scheme));
		REQUIRE(table.has_value());
		CHECK(findMismatches(scheme, *table).empty());
	}

	SECTION("Tricky names and descriptions")
	{
		const Config::Scheme scheme = {
			{ "with space", "a.b", "Multi-line\ndescription", std::string("quote\" and\nnewline") },
			{ "with space", "number", "", int64_t(-42) },
			{ "ключ", "double", "Description with # and = inside", 1.5 },
			{ "ключ", "flag", "Trailing newline\n", true },
		};

		auto table = tk::toml::parse(Config::defaultFileText(scheme));
		REQUIRE(table.has_value());
		CHECK(findMismatches(scheme, *table).empty());
		CHECK(table->size() == 2);
	}
}

} // namespace voxen::svc
