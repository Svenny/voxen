#include <tk/format.hpp>

#include <fmt/format.h>

namespace fmt
{

format_context::iterator formatter<tk::UID>::format(const tk::UID &uid, format_context &ctx) const
{
	char chars[tk::UID::CHAR_REPR_LENGTH];
	uid.toChars(chars);
	return formatter<string_view>::format(string_view(chars, tk::UID::CHAR_REPR_LENGTH - 1), ctx);
}

} // namespace fmt
