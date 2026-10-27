#include "test_common.hpp"

#include <fmt/format.h>

namespace Catch
{

std::string StringMaker<std::error_code>::convert(const std::error_code &ec)
{
	return fmt::format("[{}:{}] ({})", ec.category().name(), ec.value(), ec.message());
}

std::string StringMaker<std::error_condition>::convert(const std::error_condition &ec)
{
	return fmt::format("[{}:{}] ({})", ec.category().name(), ec.value(), ec.message());
}

} // namespace Catch
