struct Attribs {
	int16_t2 normal_oct_snorm;
	uint16_t4 mat_hist_entries;
	uint8_t4 mat_hist_weights;
};

[[vk::binding(0, 0)]] StructuredBuffer<uint4> g_in;
[[vk::binding(1, 0)]] RWStructuredBuffer<uint64_t> g_out;

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint3 id: SV_DispatchThreadID)
{
	Attribs a = bit_cast<Attribs>(g_in[id.x / 2]);
	uint64_t lo = uint64_t(uint16_t(a.normal_oct_snorm.x)) | (uint64_t(uint16_t(a.normal_oct_snorm.y)) << 16)
		| (uint64_t(a.mat_hist_entries.x) << 32) | (uint64_t(a.mat_hist_entries.y) << 48);
	uint64_t hi = uint64_t(a.mat_hist_entries.z) | (uint64_t(a.mat_hist_entries.w) << 16)
		| (uint64_t(a.mat_hist_weights.x) << 32) | (uint64_t(a.mat_hist_weights.y) << 40)
		| (uint64_t(a.mat_hist_weights.z) << 48) | (uint64_t(a.mat_hist_weights.w) << 56);
	g_out[id.x] = (id.x & 1) != 0 ? hi : lo;
}
