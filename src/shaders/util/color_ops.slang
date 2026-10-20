module color_ops;

public float3 srgbToLinear(float3 srgb)
{
	bool3 small = srgb <= 0.04045;
	float3 linear_part = srgb / 12.92;
	float3 nonlinear_part = pow((srgb + 0.055) / 1.055, float3(2.4));

	return select(small, linear_part, nonlinear_part);
}
