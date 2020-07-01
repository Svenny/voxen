#pragma once

#include <extras/dyn_array.hpp>

#include <cstddef>
#include <optional>

namespace voxen
{

class FileUtils {
public:
	static std::optional<extras::dyn_array<std::byte>> readFile(const char *path) noexcept;
	static bool writeFile(const char *path, const void *data, size_t size) noexcept;
};

}
