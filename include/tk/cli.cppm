module;

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module tk.cli;

// Command-line arguments parser.
//
// Supported syntax:
//     --name, -n            flag (`ValueType::Flag` options only)
//     --name=true|false     flag with explicit value
//     --name=value          option with value
//     --name value          same, unless the value looks like an option (starts
//                           with `-`, except negative numbers like `-1` or `-.5`)
//     -n value              short option with value, same restriction
//     --                    all the following arguments are positional
//     anything else         positional argument (including a single `-`)
//
// Options can be repeated, the last occurrence wins.

export namespace tk::cli
{

class Args;

class Parser {
public:
	enum class ValueType : uint8_t {
		// No value, presence means `true`. Can be set explicitly with `--name=true|false`.
		Flag,
		// Any string
		String,
		// 64-bit signed integer
		Integer,
		// Finite floating-point number
		Float,
	};

	struct Option {
		// Long name, used as `--name`. Letters, digits, `-`, `_` and `.`.
		std::string name;
		// Optional one-letter name, used as `-n`. Zero means no short name.
		char short_name = 0;
		ValueType type = ValueType::Flag;
		std::string description;
		// Value used when the option is not given, must be valid for `type`.
		// Flags are always `false` by default.
		std::optional<std::string> default_value;
		// Options are listed by groups in the help text, in order of group creation.
		// Empty group goes first.
		std::string group;
	};

	explicit Parser(std::string program_name, std::string description = {});

	// Option names (long and short) must be unique and valid. Invalid options
	// are programming errors, they are not reported at runtime (only asserted).
	Parser &addOption(Option option);

	// Shorthand to add a flag
	Parser &addFlag(std::string name, char short_name, std::string description, std::string group = {});

	// Parses arguments without the program name. Returns the error message on failure.
	std::expected<Args, std::string> parse(std::span<const std::string_view> args) const;
	// Parses `main()` arguments, skipping `argv[0]`. Returns the error message on failure.
	std::expected<Args, std::string> parse(int argc, const char *const *argv) const;

	// Formatted help text listing all options
	std::string helpText() const;

	const std::vector<Option> &options() const noexcept { return m_options; }

private:
	std::string m_program_name;
	std::string m_description;
	std::vector<Option> m_options;

	const Option *findLong(std::string_view name) const noexcept;
	const Option *findShort(char name) const noexcept;
};

// Result of `Parser::parse()`
class Args {
public:
	struct Entry {
		// Long option name
		std::string name;
		// Option value, flags have it normalized to `true` or `false`
		std::string value;
	};

	// Whether the option was given explicitly
	bool has(std::string_view name) const noexcept;

	// Value of the last occurrence of an option, or its default value.
	// Returns null if the option was not given and has no default value.
	std::optional<std::string_view> value(std::string_view name) const noexcept;

	// Typed value accessors. They return null if there is no value, but the
	// value is always well-formed for the type declared in `Parser::Option`.
	std::optional<int64_t> integer(std::string_view name) const noexcept;
	std::optional<double> number(std::string_view name) const noexcept;

	// Flag value, `false` if it was not given
	bool flag(std::string_view name) const noexcept;

	// Explicitly given options in command line order, including repeated ones
	const std::vector<Entry> &entries() const noexcept { return m_entries; }
	// Positional (non-option) arguments
	const std::vector<std::string> &positional() const noexcept { return m_positional; }

private:
	std::vector<Entry> m_entries;
	std::vector<Entry> m_defaults;
	std::vector<std::string> m_positional;

	friend class Parser;
};

} // namespace tk::cli
