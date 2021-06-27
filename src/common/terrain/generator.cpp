#include <voxen/common/terrain/generator.hpp>

#include <voxen/util/log.hpp>

#include <cmath>

namespace voxen
{

using ValuesArray = double[33][33][33];

template<int D, typename F>
static glm::dvec3 findZeroCrossing(double x0, double y0, double z0,
                                   double c1, double f0, double f1, F &&f) noexcept
{
	constexpr int STEP_COUNT = 6;

	glm::dvec3 p(x0, y0, z0);
	double c0 = p[D];
	int side = 0;
	double mid = c0;

	for (int i = 0; i < STEP_COUNT; i++) {
		mid = (c0 * f1 - c1 * f0) / (f1 - f0);
		p[D] = mid;
		double val = f(p.x, p.y, p.z);

		if (val * f0 > 0.0) {
			c0 = mid;
			if (side == -1) {
				double m = 1.0 - val / f0;
				if (m <= 0.0)
					m = 0.5;
				f1 *= m;
			}
			f0 = val;
			side = -1;
		} else if (val * f1 > 0.0) {
			c1 = mid;
			if (side == +1) {
				double m = 1.0 - val / f1;
				if (m <= 0.0)
					m = 0.5;
				f0 *= m;
			}
			f1 = val;
			side = +1;
		} else {
			break;
		}
	}
	return p;
}

struct ZeroCrossingContext {
	const TerrainChunkHeader &header;
	double x0, y0, z0, c1;
	double value_lesser, value_bigger;
	bool sign_lesser, sign_bigger;
	voxel_t voxel_lesser, voxel_bigger;
};

template<int D, typename F, typename DF>
static void addZeroCrossing(const ZeroCrossingContext &ctx, F &&f, DF &&df, terrain::HermiteDataStorage &storage)
{
	glm::dvec3 point_world =
		findZeroCrossing<D>(ctx.x0, ctx.y0, ctx.z0, ctx.c1, ctx.value_lesser, ctx.value_bigger, f);
	glm::dvec3 point_local = ctx.header.worldToLocal(point_world.x, point_world.y, point_world.z);
	double offset = glm::fract(point_local[D]);

	glm::vec3 normal = glm::normalize(df(point_world.x, point_world.y, point_world.z));
	bool lesser_endpoint_solid = ctx.sign_lesser;
	voxel_t solid_voxel = lesser_endpoint_solid ? ctx.voxel_lesser : ctx.voxel_bigger;

	storage.emplace(point_local.x, point_local.y, point_local.z,
	                normal, offset, D, lesser_endpoint_solid, solid_voxel);
}

static double fbase(double x, double z) noexcept
{
	const double hypot = std::hypot(x, z);
	const double amp = std::max(15.0, 0.05 * hypot);
	const double freq = 0.1 / std::sqrt(1.0 + hypot);
	return amp * (std::sin(freq * x) + std::cos(freq * z));
}

static double f(double x, double y, double z) noexcept
{
	const double base = -0.000001 * (x * x + z * z);
	return y + base + fbase(x, z) + (std::sin(0.075 * x) + std::cos(0.075 * z));
}

static glm::dvec3 df(double x, double /*y*/, double z) noexcept
{
	const double eps = 1e-6;
	double dx = (fbase(x + eps, z) - fbase(x - eps, z)) / (eps + eps) - 0.000002 * x + 0.075 * std::cos(0.075 * x);
	double dz = (fbase(x, z + eps) - fbase(x, z - eps)) / (eps + eps) - 0.000002 * z - 0.075 * std::sin(0.075 * z);
	return glm::dvec3(dx, 1.0, dz);
}

void TerrainGenerator::generate(const TerrainChunkHeader &header, TerrainChunkPrimaryData &output) const
{
	constexpr uint32_t SIZE = terrain::VoxelGrid::GRID_SIZE;
	using ValuesArray = std::array<std::array<std::array<double, SIZE>, SIZE>, SIZE>;

	Log::trace("Generating chunk at ({}, {}, {})(x{})", header.base_x, header.base_y, header.base_z, header.scale);

	auto &grid = output.voxel_grid;
	// Temporary storage for SDF values, we will need it to find zero crossings
	auto p_values = std::make_unique<ValuesArray>();
	ValuesArray &values = *p_values;

	// TODO: this is a temporary stub, add real land generator
	const uint32_t step = header.scale;

	// Fill out values and voxels arrays
	for (uint32_t i = 0; i < SIZE; i++) {
		double y = double(header.base_y + i * step);
		for (uint32_t j = 0; j < SIZE; j++) {
			double x = double(header.base_x + j * step);
			voxel_t *scanline = grid.zScanline(j, i).data();

			for (uint32_t k = 0; k < SIZE; k++) {
				double z = double(header.base_z + k * step);

				double value = f(x, y, z);
				values[i][j][k] = value;

				voxel_t voxel = 0;
				if (value <= 0.0)
					voxel = 1;
				scanline[k] = voxel;
			}
		}
	}

	// Find edges
	ZeroCrossingContext ctx {
		.header = header,
		.x0 = 0.0, .y0 = 0.0, .z0 = 0.0, .c1 = 0.0,
		.value_lesser = 0.0, .value_bigger = 0.0,
		.sign_lesser = false, .sign_bigger = false,
		.voxel_lesser = 0, .voxel_bigger = 0
	};

	const auto &voxels = grid.voxels();

	for (uint32_t i = 0; i < SIZE; i++) {
		ctx.y0 = double(header.base_y + i * step);
		for (uint32_t j = 0; j < SIZE; j++) {
			ctx.x0 = double(header.base_x + j * step);
			for (uint32_t k = 0; k < SIZE; k++) {
				ctx.z0 = double(header.base_z + k * step);
				ctx.value_lesser = values[i][j][k];
				ctx.sign_lesser = values[i][j][k] <= 0.0;
				ctx.voxel_lesser = voxels[i][j][k];

				if (i + 1 < SIZE) {
					ctx.sign_bigger = (values[i + 1][j][k] <= 0.0);
					if (ctx.sign_lesser != ctx.sign_bigger) {
						ctx.c1 = ctx.y0 + double(step);
						ctx.value_bigger = values[i + 1][j][k];
						ctx.voxel_bigger = voxels[i + 1][j][k];
						addZeroCrossing<1>(ctx, f, df, output.hermite_data_y);
					}
				}
				if (j + 1 < SIZE) {
					ctx.sign_bigger = (values[i][j + 1][k] <= 0.0);
					if (ctx.sign_lesser != ctx.sign_bigger) {
						ctx.c1 = ctx.x0 + double(step);
						ctx.value_bigger = values[i][j + 1][k];
						ctx.voxel_bigger = voxels[i][j + 1][k];
						addZeroCrossing<0>(ctx, f, df, output.hermite_data_x);
					}
				}
				if (k + 1 < SIZE) {
					ctx.sign_bigger = (values[i][j][k + 1] <= 0.0);
					if (ctx.sign_lesser != ctx.sign_bigger) {
						ctx.c1 = ctx.z0 + double(step);
						ctx.value_bigger = values[i][j][k + 1];
						ctx.voxel_bigger = voxels[i][j][k + 1];
						addZeroCrossing<2>(ctx, f, df, output.hermite_data_z);
					}
				}
			}
		}
	}
}

}
