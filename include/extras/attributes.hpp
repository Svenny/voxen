#pragma once

// MSVC still does not implement it but has its own version:
// https://github.com/llvm/llvm-project/issues/49358
// https://devblogs.microsoft.com/cppblog/msvc-cpp20-and-the-std-cpp20-switch/#c++20-[[no_unique_address]]
// ... yeah, ignoring unknown attributes is such a wise decision /s
#ifndef _WIN32
	#define EXTRAS_NO_UNIQUE_ADDRESS no_unique_address
#else
	#define EXTRAS_NO_UNIQUE_ADDRESS msvc::no_unique_address
#endif
