#include <tk/cli.hpp>

#include "text_utils.hpp"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <system_error>
#include <utility>

namespace tk::cli
{

namespace
{

// Widest left column (option names) in help text, longer ones move description to the next line
constexpr size_t MAX_HELP_NAME_WIDTH = 40;

template<typename T>
bool parseFull(std::string_view text, T &value) noexcept
{
	const char *last = text.data() + text.size();
	auto [ptr, ec] = std::from_chars(text.data(), last, value);
	return ec == std::errc() && ptr == last;
}

// Used only in asserts
[[maybe_unused]] bool isValidName(std::string_view name) noexcept
{
	if (name.empty() || name.front() == '-') {
		return false;
	}

	return std::ranges::all_of(name,
		[](char c) { return detail::isAsciiAlpha(c) || detail::isAsciiDigit(c) || c == '-' || c == '_' || c == '.'; });
}

// Does an argument look like an option rather than its value (negative numbers are values)
bool looksLikeOption(std::string_view arg) noexcept
{
	return arg.size() >= 2 && arg[0] == '-' && !detail::isAsciiDigit(arg[1]) && arg[1] != '.';
}

bool isValidValue(Parser::ValueType type, std::string_view value) noexcept
{
	int64_t int_value = 0;
	double float_value = 0;

	switch (type) {
	case Parser::ValueType::Flag:
		return value == "true" || value == "false";
	case Parser::ValueType::String:
		return true;
	case Parser::ValueType::Integer:
		return parseFull(value, int_value);
	case Parser::ValueType::Float:
		return parseFull(value, float_value) && std::isfinite(float_value);
	}

	return false;
}

const char *valuePlaceholder(Parser::ValueType type) noexcept
{
	switch (type) {
	case Parser::ValueType::Flag:
		return "";
	case Parser::ValueType::String:
		return " <string>";
	case Parser::ValueType::Integer:
		return " <int>";
	case Parser::ValueType::Float:
		return " <number>";
	}

	return "";
}

} // namespace

Parser::Parser(std::string program_name, std::string description)
	: m_program_name(std::move(program_name)), m_description(std::move(description))
{}

Parser &Parser::addOption(Option option)
{
	assert(isValidName(option.name));
	assert(option.short_name == 0 || detail::isAsciiAlpha(option.short_name) || detail::isAsciiDigit(option.short_name));
	assert(!findLong(option.name));
	assert(option.short_name == 0 || !findShort(option.short_name));
	assert(option.type != ValueType::Flag || !option.default_value.has_value());
	assert(!option.default_value.has_value() || isValidValue(option.type, *option.default_value));

	m_options.emplace_back(std::move(option));
	return *this;
}

Parser &Parser::addFlag(std::string name, char short_name, std::string description, std::string group)
{
	return addOption(Option {
		.name = std::move(name),
		.short_name = short_name,
		.type = ValueType::Flag,
		.description = std::move(description),
		.default_value = std::nullopt,
		.group = std::move(group),
	});
}

std::expected<Args, std::string> Parser::parse(std::span<const std::string_view> args) const
{
	Args result;

	for (const Option &option : m_options) {
		if (option.type == ValueType::Flag) {
			result.m_defaults.emplace_back(option.name, "false");
		} else if (option.default_value.has_value()) {
			result.m_defaults.emplace_back(option.name, *option.default_value);
		}
	}

	bool only_positional = false;
	size_t index = 0;

	while (index < args.size()) {
		const std::string_view arg = args[index++];

		if (only_positional || arg.size() < 2 || arg.front() != '-') {
			result.m_positional.emplace_back(arg);
			continue;
		}

		if (arg == "--") {
			only_positional = true;
			continue;
		}

		const Option *option = nullptr;
		std::optional<std::string_view> inline_value;

		if (arg.starts_with("--")) {
			std::string_view name = arg.substr(2);

			if (size_t eq = name.find('='); eq != std::string_view::npos) {
				inline_value = name.substr(eq + 1);
				name = name.substr(0, eq);
			}

			option = findLong(name);
		} else if (arg.size() == 2) {
			option = findShort(arg[1]);
		}

		if (!option) {
			return std::unexpected("unknown option '" + std::string(arg) + "'");
		}

		std::string value;

		if (option->type == ValueType::Flag) {
			if (!inline_value || *inline_value == "true") {
				value = "true";
			} else if (*inline_value == "false") {
				value = "false";
			} else {
				return std::unexpected("option '--" + option->name + "' accepts only 'true' or 'false' value");
			}
		} else {
			if (inline_value) {
				value = *inline_value;
			} else if (index < args.size() && !looksLikeOption(args[index])) {
				value = args[index++];
			} else if (index < args.size()) {
				return std::unexpected("option '" + std::string(arg) + "' requires a value, use '--" + option->name
					+ "=<value>' if it starts with '-'");
			} else {
				return std::unexpected("option '" + std::string(arg) + "' requires a value");
			}

			if (option->type == ValueType::Integer && !isValidValue(option->type, value)) {
				return std::unexpected("option '--" + option->name + "' requires an integer value, got '" + value + "'");
			}

			if (option->type == ValueType::Float && !isValidValue(option->type, value)) {
				return std::unexpected("option '--" + option->name + "' requires a number value, got '" + value + "'");
			}
		}

		result.m_entries.emplace_back(option->name, std::move(value));
	}

	return result;
}

std::expected<Args, std::string> Parser::parse(int argc, const char *const *argv) const
{
	std::vector<std::string_view> args;
	for (int i = 1; i < argc; i++) {
		args.emplace_back(argv[i]);
	}

	return parse(args);
}

std::string Parser::helpText() const
{
	std::vector<std::string> groups;
	// Empty (default) group always goes first
	groups.emplace_back();

	for (const Option &option : m_options) {
		if (std::ranges::find(groups, option.group) == groups.end()) {
			groups.push_back(option.group);
		}
	}

	auto make_names = [](const Option &option) {
		std::string names = option.short_name != 0 ? std::string("-") + option.short_name + ", " : "    ";
		names += "--";
		names += option.name;
		names += valuePlaceholder(option.type);
		return names;
	};

	size_t names_width = 0;
	for (const Option &option : m_options) {
		names_width = std::max(names_width, make_names(option).size());
	}
	names_width = std::min(names_width, MAX_HELP_NAME_WIDTH);

	std::string text;
	if (!m_description.empty()) {
		text += m_description;
		text += "\n\n";
	}

	text += "Usage: ";
	text += m_program_name;
	text += " [options]\n";

	for (const std::string &group : groups) {
		if (std::ranges::none_of(m_options, [&](const Option &option) { return option.group == group; })) {
			continue;
		}

		text += '\n';
		text += group.empty() ? std::string("Options") : group;
		text += ":\n";

		for (const Option &option : m_options) {
			if (option.group != group) {
				continue;
			}

			std::string names = make_names(option);

			text += "  ";
			text += names;

			if (names.size() > names_width) {
				text += '\n';
				text.append(names_width + 2, ' ');
			} else {
				text.append(names_width - names.size(), ' ');
			}

			text += "  ";
			text += option.description;

			if (option.default_value.has_value()) {
				text += " [default: ";
				text += *option.default_value;
				text += ']';
			}

			text += '\n';
		}
	}

	return text;
}

const Parser::Option *Parser::findLong(std::string_view name) const noexcept
{
	auto iter = std::ranges::find(m_options, name, &Option::name);
	return iter != m_options.end() ? &*iter : nullptr;
}

const Parser::Option *Parser::findShort(char name) const noexcept
{
	auto iter = std::ranges::find(m_options, name, &Option::short_name);
	return iter != m_options.end() ? &*iter : nullptr;
}

bool Args::has(std::string_view name) const noexcept
{
	return std::ranges::find(m_entries, name, &Entry::name) != m_entries.end();
}

std::optional<std::string_view> Args::value(std::string_view name) const noexcept
{
	// The last occurrence wins
	for (auto iter = m_entries.rbegin(); iter != m_entries.rend(); ++iter) {
		if (iter->name == name) {
			return iter->value;
		}
	}

	if (auto iter = std::ranges::find(m_defaults, name, &Entry::name); iter != m_defaults.end()) {
		return iter->value;
	}

	return std::nullopt;
}

std::optional<int64_t> Args::integer(std::string_view name) const noexcept
{
	std::optional<std::string_view> text = value(name);
	int64_t result = 0;

	if (!text || !parseFull(*text, result)) {
		return std::nullopt;
	}

	return result;
}

std::optional<double> Args::number(std::string_view name) const noexcept
{
	std::optional<std::string_view> text = value(name);
	double result = 0;

	if (!text || !parseFull(*text, result)) {
		return std::nullopt;
	}

	return result;
}

bool Args::flag(std::string_view name) const noexcept
{
	return value(name) == "true";
}

} // namespace tk::cli
