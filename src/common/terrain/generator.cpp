#include <voxen/common/terrain/generator.hpp>

#include <voxen/common/terrain/coord.hpp>
#include <voxen/util/log.hpp>

#include <bit>
#include <cmath>

namespace voxen::terrain
{

namespace
{

struct ZeroCrossingContext {
	const ChunkId id;
	double x0, y0, z0, c1;
	double value_lesser, value_bigger;
	bool sign_lesser, sign_bigger;
	voxel_t voxel_lesser, voxel_bigger;
};

}

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

template<int D, typename F, typename DF>
static void addZeroCrossing(const ZeroCrossingContext &ctx, F &&f, DF &&df, terrain::HermiteDataStorage &storage)
{
	glm::dvec3 point_world =
		findZeroCrossing<D>(ctx.x0, ctx.y0, ctx.z0, ctx.c1, ctx.value_lesser, ctx.value_bigger, f);
	glm::dvec3 point_local = CoordUtils::worldToChunkLocal(ctx.id, point_world);
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

static void doGenerate(ChunkId id, ChunkPrimaryData &output)
{
	constexpr uint32_t GRID_SIZE = VoxelGrid::GRID_SIZE;
	using ValuesArray = std::array<std::array<std::array<double, GRID_SIZE>, GRID_SIZE>, GRID_SIZE>;

	const int64_t base_x = id.base_x * int64_t(Config::CHUNK_SIZE);
	const int64_t base_y = id.base_y * int64_t(Config::CHUNK_SIZE);
	const int64_t base_z = id.base_z * int64_t(Config::CHUNK_SIZE);
	const uint32_t scale = (1u << id.lod);

	// TODO: this is a temporary stub, add real land generator
	Log::trace("Generating chunk at ({}, {}, {})(x{})", base_x, base_y, base_z, scale);

	auto &grid = output.voxel_grid;
	// Temporary storage for SDF values, we will need it to find zero crossings
	auto p_values = std::make_unique<ValuesArray>();
	ValuesArray &values = *p_values;

	// Fill out values and voxels arrays
	for (uint32_t i = 0; i < GRID_SIZE; i++) {
		double y = double(base_y + i * scale);
		for (uint32_t j = 0; j < GRID_SIZE; j++) {
			double x = double(base_x + j * scale);
			voxel_t *scanline = grid.zScanline(j, i).data();

			for (uint32_t k = 0; k < GRID_SIZE; k++) {
				double z = double(base_z + k * scale);

				double value = f(x, y, z);
				values[i][j][k] = value;

				scanline[k] = (value <= 0.0 ? 1 : 0);
			}
		}
	}

	// Find edges
	ZeroCrossingContext ctx {
		.id = id,
		.x0 = 0.0, .y0 = 0.0, .z0 = 0.0, .c1 = 0.0,
		.value_lesser = 0.0, .value_bigger = 0.0,
		.sign_lesser = false, .sign_bigger = false,
		.voxel_lesser = 0, .voxel_bigger = 0
	};

	const auto &voxels = grid.voxels();

	for (uint32_t i = 0; i < GRID_SIZE; i++) {
		ctx.y0 = double(base_y + i * scale);
		for (uint32_t j = 0; j < GRID_SIZE; j++) {
			ctx.x0 = double(base_x + j * scale);
			for (uint32_t k = 0; k < GRID_SIZE; k++) {
				ctx.z0 = double(base_z + k * scale);

				ctx.value_lesser = values[i][j][k];
				ctx.sign_lesser = values[i][j][k] <= 0.0;
				ctx.voxel_lesser = voxels[i][j][k];

				if (i + 1 < GRID_SIZE) {
					ctx.sign_bigger = (values[i + 1][j][k] <= 0.0);
					if (ctx.sign_lesser != ctx.sign_bigger) {
						ctx.c1 = ctx.y0 + double(scale);
						ctx.value_bigger = values[i + 1][j][k];
						ctx.voxel_bigger = voxels[i + 1][j][k];
						addZeroCrossing<1>(ctx, f, df, output.hermite_data_y);
					}
				}
				if (j + 1 < GRID_SIZE) {
					ctx.sign_bigger = (values[i][j + 1][k] <= 0.0);
					if (ctx.sign_lesser != ctx.sign_bigger) {
						ctx.c1 = ctx.x0 + double(scale);
						ctx.value_bigger = values[i][j + 1][k];
						ctx.voxel_bigger = voxels[i][j + 1][k];
						addZeroCrossing<0>(ctx, f, df, output.hermite_data_x);
					}
				}
				if (k + 1 < GRID_SIZE) {
					ctx.sign_bigger = (values[i][j][k + 1] <= 0.0);
					if (ctx.sign_lesser != ctx.sign_bigger) {
						ctx.c1 = ctx.z0 + double(scale);
						ctx.value_bigger = values[i][j][k + 1];
						ctx.voxel_bigger = voxels[i][j][k + 1];
						addZeroCrossing<2>(ctx, f, df, output.hermite_data_z);
					}
				}
			}
		}
	}
}

void TerrainGenerator::generate(ChunkId id, ChunkPrimaryData &output) const
{
	doGenerate(id, output);
}

void TerrainGenerator::generate(const TerrainChunkHeader &header, ChunkPrimaryData &output) const
{
	doGenerate(ChunkId {
		.lod = uint32_t(std::countr_zero(header.scale)),
		.base_x = int32_t(header.base_x / int64_t(Config::CHUNK_SIZE)),
		.base_y = int32_t(header.base_y / int64_t(Config::CHUNK_SIZE)),
		.base_z = int32_t(header.base_z / int64_t(Config::CHUNK_SIZE))
	}, output);
}

}
