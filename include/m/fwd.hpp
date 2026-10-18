#pragma once

#include <cstddef>
#include <cstdint>

// Forward declarations and type aliases of the `m` math library.
// Headers that only mention the types should include this one,
// code that uses them includes `m/m.hpp` or the specific headers.

namespace m
{

template<typename T, size_t N>
struct vector;

template<typename T, size_t R, size_t C>
struct matrix;

template<typename T>
struct quaternion;

// Aliases follow HLSL naming: component type, then shape.
// Matrix shape is RxC - R rows of C components each.

using float2 = vector<float, 2>;
using float3 = vector<float, 3>;
using float4 = vector<float, 4>;

using double2 = vector<double, 2>;
using double3 = vector<double, 3>;
using double4 = vector<double, 4>;

using int2 = vector<int32_t, 2>;
using int3 = vector<int32_t, 3>;
using int4 = vector<int32_t, 4>;

using uint2 = vector<uint32_t, 2>;
using uint3 = vector<uint32_t, 3>;
using uint4 = vector<uint32_t, 4>;

using bool2 = vector<bool, 2>;
using bool3 = vector<bool, 3>;
using bool4 = vector<bool, 4>;

using int8_t2 = vector<int8_t, 2>;
using int8_t3 = vector<int8_t, 3>;
using int8_t4 = vector<int8_t, 4>;

using uint8_t2 = vector<uint8_t, 2>;
using uint8_t3 = vector<uint8_t, 3>;
using uint8_t4 = vector<uint8_t, 4>;

using int16_t2 = vector<int16_t, 2>;
using int16_t3 = vector<int16_t, 3>;
using int16_t4 = vector<int16_t, 4>;

using uint16_t2 = vector<uint16_t, 2>;
using uint16_t3 = vector<uint16_t, 3>;
using uint16_t4 = vector<uint16_t, 4>;

using int64_t2 = vector<int64_t, 2>;
using int64_t3 = vector<int64_t, 3>;
using int64_t4 = vector<int64_t, 4>;

using uint64_t2 = vector<uint64_t, 2>;
using uint64_t3 = vector<uint64_t, 3>;
using uint64_t4 = vector<uint64_t, 4>;

using float2x2 = matrix<float, 2, 2>;
using float2x3 = matrix<float, 2, 3>;
using float2x4 = matrix<float, 2, 4>;
using float3x2 = matrix<float, 3, 2>;
using float3x3 = matrix<float, 3, 3>;
using float3x4 = matrix<float, 3, 4>;
using float4x2 = matrix<float, 4, 2>;
using float4x3 = matrix<float, 4, 3>;
using float4x4 = matrix<float, 4, 4>;

using double2x2 = matrix<double, 2, 2>;
using double2x3 = matrix<double, 2, 3>;
using double2x4 = matrix<double, 2, 4>;
using double3x2 = matrix<double, 3, 2>;
using double3x3 = matrix<double, 3, 3>;
using double3x4 = matrix<double, 3, 4>;
using double4x2 = matrix<double, 4, 2>;
using double4x3 = matrix<double, 4, 3>;
using double4x4 = matrix<double, 4, 4>;

// HLSL has no quaternions, the names continue its "type, then shape" pattern
using fquat = quaternion<float>;
using dquat = quaternion<double>;

} // namespace m
