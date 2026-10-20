module;

#include <string>

module tk.parse_error;

namespace tk
{

std::string ParseError::toString() const
{
	return std::to_string(line) + ":" + std::to_string(column) + ": " + message;
}

} // namespace tk
