#ifndef VX_UTIL_COLOR_OPS_GLSL
#define VX_UTIL_COLOR_OPS_GLSL

vec3 srgbToLinear(vec3 srgb)
{
	bvec3 small = lessThanEqual(srgb, vec3(0.04045));
	vec3 linear_part = srgb / 12.92;
	vec3 nonlinear_part = pow((srgb + 0.055) / 1.055, vec3(2.4));

	return mix(nonlinear_part, linear_part, small);
}

#endif // VX_UTIL_COLOR_OPS_GLSL
