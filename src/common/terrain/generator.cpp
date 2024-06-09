#include <voxen/common/terrain/generator.hpp>

#include <voxen/util/log.hpp>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include <bit>
#include <cmath>

namespace voxen::terrain
{

namespace
{

struct ZeroCrossingContext {
	HermiteDataEntry::coord_t store_x, store_y, store_z;
	glm::dvec3 edge_world_min;
	double edge_world_length;
	double value_lesser, value_bigger;
	bool sign_lesser, sign_bigger;
	voxel_t voxel_lesser, voxel_bigger;
};

} // namespace

template<int D, typename F>
static glm::dvec3 findZeroCrossing(glm::dvec3 p, double c1, double f0, double f1, F &&f) noexcept
{
	constexpr int STEP_COUNT = 6;

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
				if (m <= 0.0) {
					m = 0.5;
				}
				f1 *= m;
			}
			f0 = val;
			side = -1;
		} else if (val * f1 > 0.0) {
			c1 = mid;
			if (side == +1) {
				double m = 1.0 - val / f1;
				if (m <= 0.0) {
					m = 0.5;
				}
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
static void addZeroCrossing(const ZeroCrossingContext &ctx, F &&f, DF &&df, HermiteDataStorage &storage)
{
	const double edge_world_min_c = ctx.edge_world_min[D];
	const double edge_world_max_c = edge_world_min_c + ctx.edge_world_length;

	glm::dvec3 point_world = findZeroCrossing<D>(ctx.edge_world_min, edge_world_max_c, ctx.value_lesser,
		ctx.value_bigger, f);
	double offset = (point_world[D] - edge_world_min_c) / ctx.edge_world_length;

	glm::vec3 normal = glm::normalize(df(point_world.x, point_world.y, point_world.z));
	bool lesser_endpoint_solid = ctx.sign_lesser;
	voxel_t solid_voxel = lesser_endpoint_solid ? ctx.voxel_lesser : ctx.voxel_bigger;

	storage.emplace(ctx.store_x, ctx.store_y, ctx.store_z, normal, offset, D, lesser_endpoint_solid, solid_voxel);
}

constexpr static double MOUNTAIN_SPREAD = 10'000.0;
constexpr static double MOUNTAIN_THINNESS = 1.0 / 490'000.0;
constexpr static double MOUNTAIN_BASE_HEIGHT = 550.0;

static double fbase(double x, double z) noexcept
{
	const double len = glm::length(glm::dvec2(x, z));
	const double order = glm::round(len / MOUNTAIN_SPREAD);
	const double pivot = order * MOUNTAIN_SPREAD;
	const double offset = glm::abs(len - pivot);
	return -MOUNTAIN_BASE_HEIGHT * glm::exp2(-MOUNTAIN_THINNESS * offset * offset);
}

static double f(double x, double y, double z) noexcept
{
	return y + fbase(x, z) - 20.0;
}

static glm::dvec3 df(double x, double /*y*/, double z) noexcept
{
	const double eps = 1e-6;
	double dx = (fbase(x + eps, z) - fbase(x - eps, z)) / (eps + eps);
	double dz = (fbase(x, z + eps) - fbase(x, z - eps)) / (eps + eps);
	return glm::dvec3(dx, 1.0, dz);
}

void TerrainGenerator::generate(ChunkId id, ChunkPrimaryData &output) const
{
	constexpr uint32_t GRID_SIZE = VoxelGrid::GRID_SIZE;
	using coord_t = HermiteDataEntry::coord_t;
	using ValuesArray = std::array<std::array<std::array<double, GRID_SIZE>, GRID_SIZE>, GRID_SIZE>;

	const int64_t base_x = id.base_x * int64_t(Config::CHUNK_SIZE);
	const int64_t base_y = id.base_y * int64_t(Config::CHUNK_SIZE);
	const int64_t base_z = id.base_z * int64_t(Config::CHUNK_SIZE);
	const uint32_t scale = (1u << id.lod);

	// TODO: this is a temporary stub, add real land generator

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

	// Find surface-crossing edges
	ZeroCrossingContext ctx {};
	ctx.edge_world_length = double(scale);

	const auto &voxels = grid.voxels();

	for (uint32_t i = 0; i < GRID_SIZE; i++) {
		ctx.store_y = coord_t(i);
		ctx.edge_world_min.y = double(base_y + i * scale);

		for (uint32_t j = 0; j < GRID_SIZE; j++) {
			ctx.store_x = coord_t(j);
			ctx.edge_world_min.x = double(base_x + j * scale);

			for (uint32_t k = 0; k < GRID_SIZE; k++) {
				ctx.store_z = coord_t(k);
				ctx.edge_world_min.z = double(base_z + k * scale);

				ctx.value_lesser = values[i][j][k];
				ctx.sign_lesser = values[i][j][k] <= 0.0;
				ctx.voxel_lesser = voxels[i][j][k];

				if (i + 1 < GRID_SIZE) {
					ctx.sign_bigger = (values[i + 1][j][k] <= 0.0);
					if (ctx.sign_lesser != ctx.sign_bigger) {
						ctx.value_bigger = values[i + 1][j][k];
						ctx.voxel_bigger = voxels[i + 1][j][k];
						addZeroCrossing<1>(ctx, f, df, output.hermite_data_y);
					}
				}
				if (j + 1 < GRID_SIZE) {
					ctx.sign_bigger = (values[i][j + 1][k] <= 0.0);
					if (ctx.sign_lesser != ctx.sign_bigger) {
						ctx.value_bigger = values[i][j + 1][k];
						ctx.voxel_bigger = voxels[i][j + 1][k];
						addZeroCrossing<0>(ctx, f, df, output.hermite_data_x);
					}
				}
				if (k + 1 < GRID_SIZE) {
					ctx.sign_bigger = (values[i][j][k + 1] <= 0.0);
					if (ctx.sign_lesser != ctx.sign_bigger) {
						ctx.value_bigger = values[i][j][k + 1];
						ctx.voxel_bigger = voxels[i][j][k + 1];
						addZeroCrossing<2>(ctx, f, df, output.hermite_data_z);
					}
				}
			}
		}
	}
}

} // namespace voxen::terrain
