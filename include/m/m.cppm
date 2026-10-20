// Module interface of the `m` math library: `import m;` instead of `#include <m/m.hpp>`.
//
// This is a wrapper, declarations stay in the headers, attached to the global module.
// `import m;` and `#include <m/...>` can be mixed freely, even in the same TU, and both
// name the same entities. Macros are not exported (`m` has no public ones anyway).
// `m/format.hpp` is not wrapped as it depends on fmt, keep including it.
//
// Keep the list in sync with the headers: a missing name is a compile error
// for importers only, header users will not notice.

module;

#include <m/m.hpp>

export module m;

export namespace m
{

// Concepts
using m::CComponent;
using m::CFloating;
using m::CIntegral;
using m::CScalar;
using m::CSnormInteger;
using m::CUnormInteger;
using m::CVector;

// Class templates and aliases
using m::matrix;
using m::quaternion;
using m::vector;

using m::bool2;
using m::bool3;
using m::bool4;
using m::double2;
using m::double2x2;
using m::double2x3;
using m::double2x4;
using m::double3;
using m::double3x2;
using m::double3x3;
using m::double3x4;
using m::double4;
using m::double4x2;
using m::double4x3;
using m::double4x4;
using m::dquat;
using m::float2;
using m::float2x2;
using m::float2x3;
using m::float2x4;
using m::float3;
using m::float3x2;
using m::float3x3;
using m::float3x4;
using m::float4;
using m::float4x2;
using m::float4x3;
using m::float4x4;
using m::fquat;
using m::int16_t2;
using m::int16_t3;
using m::int16_t4;
using m::int2;
using m::int3;
using m::int4;
using m::int64_t2;
using m::int64_t3;
using m::int64_t4;
using m::int8_t2;
using m::int8_t3;
using m::int8_t4;
using m::uint16_t2;
using m::uint16_t3;
using m::uint16_t4;
using m::uint2;
using m::uint3;
using m::uint4;
using m::uint64_t2;
using m::uint64_t3;
using m::uint64_t4;
using m::uint8_t2;
using m::uint8_t3;
using m::uint8_t4;

// Functions
using m::abs;
using m::all;
using m::any;
using m::ceil;
using m::clamp;
using m::column;
using m::conjugate;
using m::cos;
using m::cross;
using m::degrees;
using m::determinant;
using m::distance;
using m::distanceSquared;
using m::dot;
using m::equal;
using m::floor;
using m::floorDiv;
using m::floorMod;
using m::frac;
using m::greaterThan;
using m::greaterThanEqual;
using m::homogeneous;
using m::inverse;
using m::inverseAffine;
using m::isfinite;
using m::isnan;
using m::length;
using m::lengthSquared;
using m::lerp;
using m::lessThan;
using m::lessThanEqual;
using m::max;
using m::min;
using m::mul;
using m::normalize;
using m::notEqual;
using m::packSnorm;
using m::packUnorm;
using m::perspectiveInfinite;
using m::radians;
using m::rigidTransform;
using m::rotation;
using m::round;
using m::rsqrt;
using m::saturate;
using m::scaling;
using m::select;
using m::sign;
using m::sin;
using m::slerp;
using m::smoothstep;
using m::sqrt;
using m::step;
using m::tan;
using m::toMatrix;
using m::translation;
using m::transpose;
using m::trunc;
using m::unpackSnorm;
using m::unpackUnorm;
using m::viewFromCamera;

// Operators (found by ADL, but still must be made reachable from the module)
using m::operator!;
using m::operator%;
using m::operator%=;
using m::operator&;
using m::operator&=;
using m::operator*;
using m::operator*=;
using m::operator+;
using m::operator+=;
using m::operator-;
using m::operator-=;
using m::operator/;
using m::operator/=;
using m::operator<<;
using m::operator<<=;
using m::operator>>;
using m::operator>>=;
using m::operator^;
using m::operator^=;
using m::operator|;
using m::operator|=;
using m::operator~;

} // namespace m
