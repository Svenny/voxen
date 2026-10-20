module aabb;

public struct Aabb {
	float3 min;
	float3 max;
};

// Frustum cull implementation has two variants (0, 1),
// whichever is in general better needs to be evaluated
#define AABB_FRUSTUM_CULL_IMPL 0

// Returns `true` if given AABB is at least partially inside the NDC volume when transformed by given matrix
public bool isAabbInFrustum(Aabb aabb, float4x4 mtx)
{
	// Matrices transform column vectors: `mul(mtx, v)`.
	// Row `i` of the transpose is column `i` of the matrix.
	float4x4 cols = transpose(mtx);

#if AABB_FRUSTUM_CULL_IMPL == 0
	float4 partial_ndc_min[3] = { cols[0] * aabb.min.x, cols[1] * aabb.min.y, cols[2] * aabb.min.z };
	float4 partial_ndc_max[3] = { cols[0] * aabb.max.x, cols[1] * aabb.max.y, cols[2] * aabb.max.z };
#endif // AABB_FRUSTUM_CULL_IMPL == 0

	bool inside[6] = { false, false, false, false, false, false };

	for (uint i = 0; i < 8; i++) {
		float4 ndc = cols[3];
#if AABB_FRUSTUM_CULL_IMPL == 0
		ndc += ((i & 1) == 0) ? partial_ndc_min[0] : partial_ndc_max[0];
		ndc += ((i & 2) == 0) ? partial_ndc_min[1] : partial_ndc_max[1];
		ndc += ((i & 4) == 0) ? partial_ndc_min[2] : partial_ndc_max[2];
#else
		ndc = mad(cols[0], float4(((i & 1) == 0) ? aabb.min.x : aabb.max.x), ndc);
		ndc = mad(cols[1], float4(((i & 2) == 0) ? aabb.min.y : aabb.max.y), ndc);
		ndc = mad(cols[2], float4(((i & 4) == 0) ? aabb.min.z : aabb.max.z), ndc);
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
