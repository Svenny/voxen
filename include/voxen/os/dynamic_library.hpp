#pragma once

#include <voxen/visibility.hpp>

namespace voxen::os
{

// A loaded shared library (`.so`/`.dll`) with `std::unique_ptr`-like semantics
class VOXEN_API DynamicLibrary {
public:
	DynamicLibrary() = default;
	DynamicLibrary(DynamicLibrary &&other) noexcept;
	DynamicLibrary(const DynamicLibrary &) = delete;
	DynamicLibrary &operator=(DynamicLibrary &&other) noexcept;
	DynamicLibrary &operator=(const DynamicLibrary &) = delete;
	~DynamicLibrary() noexcept;

	// Load a library by file name (searched for as the OS does it) or path.
	// Returns an empty object and logs the reason if it can't be loaded.
	static DynamicLibrary open(const char *name) noexcept;

	// Address of an exported symbol, null if there is no such symbol
	void *symbol(const char *name) const noexcept;

	bool valid() const noexcept { return m_handle != nullptr; }

private:
	void *m_handle = nullptr;
};

} // namespace voxen::os
