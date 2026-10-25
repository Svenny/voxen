#include <voxen/os/dynamic_library.hpp>

#include <voxen/util/log.hpp>

#ifndef _WIN32
	#include <dlfcn.h>
#else
	#include <Windows.h>
#endif

#include <utility>

namespace voxen::os
{

DynamicLibrary::DynamicLibrary(DynamicLibrary &&other) noexcept : m_handle(std::exchange(other.m_handle, nullptr)) {}

DynamicLibrary &DynamicLibrary::operator=(DynamicLibrary &&other) noexcept
{
	std::swap(m_handle, other.m_handle);
	return *this;
}

DynamicLibrary::~DynamicLibrary() noexcept
{
	if (!m_handle) {
		return;
	}

#ifndef _WIN32
	dlclose(m_handle);
#else
	FreeLibrary(static_cast<HMODULE>(m_handle));
#endif
}

DynamicLibrary DynamicLibrary::open(const char *name) noexcept
{
	DynamicLibrary lib;

#ifndef _WIN32
	lib.m_handle = dlopen(name, RTLD_NOW | RTLD_LOCAL);
	if (!lib.m_handle) {
		Log::error("Can't load '{}': {}", name, dlerror());
	}
#else
	lib.m_handle = LoadLibraryA(name);
	if (!lib.m_handle) {
		Log::error("Can't load '{}': error {}", name, GetLastError());
	}
#endif

	return lib;
}

void *DynamicLibrary::symbol(const char *name) const noexcept
{
#ifndef _WIN32
	return dlsym(m_handle, name);
#else
	return reinterpret_cast<void *>(GetProcAddress(static_cast<HMODULE>(m_handle), name));
#endif
}

} // namespace voxen::os
