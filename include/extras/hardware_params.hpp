#pragma once

#include <cstddef>

namespace extras
{

// Portable access to hardware parameters
class hardware_params
{
public:
	// Size of CPU cache line. Spread concurrently accessed objects
	// by this value to avoid excessive cache coherency traffic.
	constexpr static size_t cache_line = 64;
};

}
