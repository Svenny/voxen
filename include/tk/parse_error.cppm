module;

#include <cstdint>
#include <string>

export module tk.parse_error;

export namespace tk
{

// Error from parsing a text format (TOML, JSON etc.)
struct ParseError {
	// Human-readable description, without location
	std::string message;
	// 1-based line number
	uint32_t line = 0;
	// 1-based column number, counted in Unicode code points
	uint32_t column = 0;

	// Formats the error as "<line>:<column>: <message>"
	std::string toString() const;
};

} // namespace tk
