#ifndef VX_UTIL_AABB_GLSL
#define VX_UTIL_AABB_GLSL

// Mirroring CPU-side `voxen::Aabb` structure
struct Aabb {
	vec3 min;
	vec3 max;
};

// Frustum cull implementation has two variants (0, 1),
// whichever is in general better needs to be evaluated
#define AABB_FRUSTUM_CULL_IMPL 0

// Returns `true` if given AABB is at least partially inside the NDC volume when transformed by given matrix
bool isAabbInFrustum(in Aabb aabb, in mat4 mtx)
{
#if AABB_FRUSTUM_CULL_IMPL == 0
	vec4 partial_ndc_min[3] = { mtx[0] * aabb.min.x, mtx[1] * aabb.min.y, mtx[2] * aabb.min.z };
	vec4 partial_ndc_max[3] = { mtx[0] * aabb.max.x, mtx[1] * aabb.max.y, mtx[2] * aabb.max.z };
#endif // AABB_FRUSTUM_CULL_IMPL == 0

	bool inside[6] = { false, false, false, false, false, false };

	for (uint i = 0u; i < 8u; i++) {
		vec4 ndc = mtx[3];
#if AABB_FRUSTUM_CULL_IMPL == 0
		ndc += ((i & 1u) == 0u) ? partial_ndc_min[0] : partial_ndc_max[0];
		ndc += ((i & 2u) == 0u) ? partial_ndc_min[1] : partial_ndc_max[1];
		ndc += ((i & 4u) == 0u) ? partial_ndc_min[2] : partial_ndc_max[2];
#else
		ndc = fma(mtx[0], vec4(((i & 1u) == 0u) ? aabb.min.x : aabb.max.x), ndc);
		ndc = fma(mtx[1], vec4(((i & 2u) == 0u) ? aabb.min.y : aabb.max.y), ndc);
		ndc = fma(mtx[2], vec4(((i & 4u) == 0u) ? aabb.min.z : aabb.max.z), ndc);
#endif // AABB_FRUSTUM_CULL_IMPL == 0

		inside[0] = inside[0] || (ndc.z >= 0.0);
		inside[1] = inside[1] || (ndc.z <= ndc.w);
		inside[2] = inside[2] || (ndc.x >= -ndc.w);
		inside[3] = inside[3] || (ndc.x <= ndc.w);
		inside[4] = inside[4] || (ndc.y >= -ndc.w);
		inside[5] = inside[5] || (ndc.y <= ndc.w);
	}

	return inside[0] && inside[1] && inside[2] && inside[3] && inside[4] && inside[5];
}

#undef AABB_FRUSTUM_CULL_IMPL

#endif // VX_UTIL_AABB_GLSL
