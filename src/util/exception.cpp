#include <voxen/util/exception.hpp>

#include <fmt/format.h>

#include <cstring>

namespace voxen
{

Exception::Exception(const std::experimental::source_location &loc) : m_where(loc) {
	// TODO: this is the best place to print stack trace
}

ErrnoException::ErrnoException(int code, const std::experimental::source_location &loc)
   : Exception(loc) {
	char buf[1024];
	m_message = fmt::format("Error code {}: {}", code, strerror_r(code, buf, 1024));
}

}
