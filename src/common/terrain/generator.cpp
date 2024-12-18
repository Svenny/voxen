#include <voxen/common/terrain/generator.hpp>

#include <voxen/common/terrain/primary_data.hpp>
#include <voxen/util/log.hpp>

#include <extras/enum_utils.hpp>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include <cmath>
#include <mutex>

namespace voxen::terrain
{

namespace
{

enum class Biome : uint8_t {
	Plains,
	Hills,
	Snow,
	Desert,
	Mountains,

	EnumSize
};

enum class Material : voxel_t {
	Air = 0,
	Ground,
	Grass,
	Rock,
	Snow,
	Sand,
	Clay,

	FirstOre,
	IronOre = FirstOre,
	CopperOre,
	Coal,
	GoldOre,
	WolframOre,
	TitaniumOre,
	UraniumOre,

	EnumSize
};

constexpr auto NUM_ORES = extras::enum_size_v<Material> - extras::to_underlying(Material::FirstOre);

// Planet is assumed to be a spheroid (defined by polar end equatorial radii).
// The sun is assumed to be an ideal sphere radiating some power.
// Planet's orbit is assumed to be an ideal circle with constant speed.
// Planet has its own rotation with axis tilted relative to ecliptic plane.
// These parameters allow to simulate solar irradiation and planet tectonics to generate L0 surface map.
// Default values are set up to generate Earth-like planet with Sun-like sun.
struct PlanetConfig {
	// Distance from planet center to its pole (meters)
	double polar_radius = 6'356'752.0;
	// Distance from planet center to its equator (meters)
	double equatorial_radius = 6'378'137.0;
	// Planet mass (kilograms)
	double mass = 5.97217e24;
	// Mean distance from planet center to the sun (meters)
	double mean_sun_distance = 149'598'023'000.0;
	// Radius of sun (meters)
	double sun_radius = 695'700'000.0;
	// Period of planetary orbit around the sun (seconds)
	double orbital_period = 365.25 * 86400.0;
	// Angle between the axis of planetary rotation and its ecliptic plane (degrees)
	double axial_tilt = 23.4392811;
	// Period of planetary rotation (seconds)
	double rotation_period = 86400.0;

	// Abstract measure ofr sun's radiated power (1.0 for standard Sun)
	double sun_luminosity = 1.0;
	// Abstract measure of planet's tectonic activity (1.0 for standard Earth)
	double tectonic_activity = 1.0;
	// Fraction of planet's surface covered by ocean (0.71 for standard Earth)
	double water_fraction = 0.71;
};

struct ToroidalPlanetConfig {
	double inner_radius = 8'633'000.0;
	double outer_radius = 19'937'000.0;
	double pole_radius = 4'070'000.0;
	double mass = 3.583302e+25;
	double mean_sun_distance = 149'598'023'000.0;
	double sun_radius = 695'700'000.0;
	double orbital_period = 365.25 * 86400.0;
	double axial_tilt = 23.4392811;
	double rotation_period = 12708.0;
};

struct ClimateMapL0 {};

struct LandMapL0 {
	double tectonic_mass;
	double tectonic_velocity_x;
	double tectonic_velocity_y;
	float ore_distribution[NUM_ORES];
};

struct OreDeposit {
	Material ore;
	glm::vec3 center;
	glm::vec3 radii;
};

struct LandMapL1 {
	uint32_t rng_seed;
};

struct ZeroCrossingContext {
	HermiteDataEntry::coord_t store_x, store_y, store_z;
	glm::dvec3 edge_world_min;
	double edge_world_length;
	double value_lesser, value_bigger;
	bool sign_lesser, sign_bigger;
	voxel_t voxel_lesser, voxel_bigger;
};

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
void addZeroCrossing(const ZeroCrossingContext &ctx, F &&f, DF &&df, HermiteDataStorage &storage)
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

static TerrainL0Map *g_map;

#if 0
constexpr double MOUNTAIN_SPREAD = 10'000.0;
constexpr double MOUNTAIN_THINNESS = 1.0 / 490'000.0;
constexpr double MOUNTAIN_BASE_HEIGHT = 550.0;

double fbase(double x, double z) noexcept
{
	const double len = glm::length(glm::dvec2(x, z));
	const double order = glm::round(len / MOUNTAIN_SPREAD);
	const double pivot = order * MOUNTAIN_SPREAD;
	const double offset = glm::abs(len - pivot);
	return -MOUNTAIN_BASE_HEIGHT * glm::exp2(-MOUNTAIN_THINNESS * offset * offset);
}
#else
double fbase(double x, double z) noexcept
{
	return -g_map->sampleHeight(x, z);
}
#endif

double f(double x, double y, double z) noexcept
{
	return y + fbase(x, z);
}

glm::dvec3 df(double x, double /*y*/, double z) noexcept
{
	const double eps = 1e-3;
	double dx = (fbase(x + eps, z) - fbase(x - eps, z)) / (eps + eps);
	double dz = (fbase(x, z + eps) - fbase(x, z - eps)) / (eps + eps);
	return { dx, 1.0, dz };
}

} // anonymous namespace

constexpr static double L0_WIDTH = 8.975530211306038e+7;
constexpr static double L0_HEIGHT = 3.0745084822905876e+7;
constexpr static float L0_HEIGHT_MOD = 34'000.0;

TerrainL1Map::TerrainL1Map(int32_t width, int32_t height) : m_width(width), m_height(height)
{
	assert(width > 0 && width < INT16_MAX);
	assert(height > 0 && height < INT16_MAX);
	m_points = std::make_unique<Point[]>(numPoints());
}

void TerrainL1Map::init(const TerrainL0Map &l0map, uint32_t seed, double center_x, double center_y, double world_size_x,
	double world_size_y)
{
	m_world_left_x = center_x - world_size_x * 0.5;
	m_world_upper_y = center_y - world_size_y * 0.5;
	m_world_size_x = world_size_x;
	m_world_size_y = world_size_y;

	int32_t step = 32;
	assert(m_width % step == 1);
	assert(m_height % step == 1);

	// Make any accidental usage of non-filled values immediately noticeable
	for (size_t i = 0; i < numPoints(); i++) {
		Point &p = m_points[i];
		p.height = std::numeric_limits<float>::quiet_NaN();
	}

	// Initialize main points by sampling L0 map
	for (int32_t y = 0; y < m_height; y += step) {
		for (int32_t x = 0; x < m_width; x += step) {
			double u = (x + 0.5) / double(m_width);
			double v = (y + 0.5) / double(m_height);

			double wx = center_x + (u - 0.5) * world_size_x;
			double wy = center_y + (v - 0.5) * world_size_y;

			mutPoint(x, y).height = l0map.sampleSelfHeight(wx, wy);
		}
	}

	std::ranlux24 rng(seed);

	auto sample = [this](int32_t x, int32_t y, float &minval, float &maxval) {
		if (x < 0 || y < 0 || x >= m_width || y >= m_height) {
			return;
		}

		const Point &p = point(x, y);
		assert(!std::isnan(p.height));
		minval = std::min(minval, p.height);
		maxval = std::max(maxval, p.height);
	};

	while (step > 1) {
		const int32_t hstep = step / 2;

		// Diamond step
		for (int32_t y = hstep; y < m_height; y += step) {
			for (int32_t x = hstep; x < m_width; x += step) {
				float minval = std::numeric_limits<float>::max();
				float maxval = std::numeric_limits<float>::lowest();
				sample(x - hstep, y - hstep, minval, maxval);
				sample(x + hstep, y - hstep, minval, maxval);
				sample(x - hstep, y + hstep, minval, maxval);
				sample(x + hstep, y + hstep, minval, maxval);

				Point &p = mutPoint(x, y);
				assert(std::isnan(p.height));

				float range = maxval - minval;
				maxval += 0.1f * range;
				minval -= 0.1f * range;
				p.height = minval + std::generate_canonical<float, 24>(rng) * (maxval - minval);
			}
		}

		// Square step, x-shifted
		for (int32_t y = 0; y < m_height; y += step) {
			for (int32_t x = hstep; x < m_width; x += step) {
				float minval = std::numeric_limits<float>::max();
				float maxval = std::numeric_limits<float>::lowest();
				sample(x - hstep, y, minval, maxval);
				sample(x + hstep, y, minval, maxval);
				sample(x, y - hstep, minval, maxval);
				sample(x, y + hstep, minval, maxval);

				Point &p = mutPoint(x, y);
				assert(std::isnan(p.height));

				float range = maxval - minval;
				maxval += 0.1f * range;
				minval -= 0.1f * range;
				p.height = minval + std::generate_canonical<float, 24>(rng) * (maxval - minval);
			}
		}

		// Square step, y-shifted
		for (int32_t y = hstep; y < m_height; y += step) {
			for (int32_t x = 0; x < m_width; x += step) {
				float minval = std::numeric_limits<float>::max();
				float maxval = std::numeric_limits<float>::lowest();
				sample(x - hstep, y, minval, maxval);
				sample(x + hstep, y, minval, maxval);
				sample(x, y - hstep, minval, maxval);
				sample(x, y + hstep, minval, maxval);

				Point &p = mutPoint(x, y);
				assert(std::isnan(p.height));

				float range = maxval - minval;
				maxval += 0.1f * range;
				minval -= 0.1f * range;
				p.height = minval + std::generate_canonical<float, 24>(rng) * (maxval - minval);
			}
		}

		step /= 2;
	}
}

const TerrainL1Map::Point &TerrainL1Map::point(int32_t x, int32_t y) const noexcept
{
	assert(x >= 0 && x < m_width);
	assert(y >= 0 && y < m_height);
	return m_points[size_t(y * m_width + x)];
}

double TerrainL1Map::sampleHeight(double x, double y) const noexcept
{
	x = std::clamp(x - m_world_left_x, 0.0, m_world_size_x) * (m_width / m_world_size_x) - 0.5;
	y = std::clamp(y - m_world_upper_y, 0.0, m_world_size_y) * (m_height / m_world_size_y) - 0.5;

	double xf = std::floor(x);
	double yf = std::floor(y);

	auto x0 = std::max(static_cast<int32_t>(xf), 0);
	auto y0 = std::max(static_cast<int32_t>(yf), 0);

	int32_t x1 = std::min(x0 + 1, m_width - 1);
	int32_t y1 = std::min(y0 + 1, m_height - 1);

	const Point &lu = point(x0, y0);
	const Point &ru = point(x1, y0);
	const Point &ld = point(x0, y1);
	const Point &rd = point(x1, y1);

	float tx = static_cast<float>(x - xf);
	float ty = static_cast<float>(y - yf);

	float hy0 = glm::mix(lu.height, ru.height, tx);
	float hy1 = glm::mix(ld.height, rd.height, tx);
	return static_cast<double>(glm::mix(hy0, hy1, ty));
}

TerrainL1Map::Point &TerrainL1Map::mutPoint(int32_t x, int32_t y) noexcept
{
	return m_points[size_t(y * m_width + x)];
}

size_t TerrainL1Map::numPoints() const noexcept
{
	return size_t(m_width * m_height);
}

TerrainL0Map::TerrainL0Map(int32_t width, int32_t height) : m_width(width), m_height(height)
{
	assert(width > 0 && width < INT16_MAX);
	assert(height > 0 && height < INT16_MAX);
	m_points = std::make_unique<Point[]>(numPoints());
}

void TerrainL0Map::init(std::mt19937 &rng)
{
	int32_t step = 64;
	assert(m_width % step == 0);
	assert(m_height % step == 0);

	// Make any accidental usage of non-filled values immediately noticeable
	for (size_t i = 0; i < numPoints(); i++) {
		Point &p = m_points[i];
		p.height = std::numeric_limits<float>::quiet_NaN();
		p.l1seed = static_cast<uint32_t>(rng());
	}

	// Initialize main points
	for (int32_t y = 0; y < m_height; y += step) {
		for (int32_t x = 0; x < m_width; x += step) {
			mutPoint(x, y).height = (std::generate_canonical<float, 24>(rng) * 1.7f - 1.0f) * L0_HEIGHT_MOD;
		}
	}

	auto sample = [this](int32_t x, int32_t y) -> const Point & {
		const Point &p = m_points[pointIdxWrapped(x, y)];
		assert(!std::isnan(p.height));
		return p;
	};

	float magnitude = 1.0f * L0_HEIGHT_MOD;
	while (step > 1) {
		const int32_t hstep = step / 2;

		// Diamond step
		for (int32_t y = hstep; y < m_height; y += step) {
			for (int32_t x = hstep; x < m_width; x += step) {
				const auto &lu = sample(x - hstep, y - hstep);
				const auto &ru = sample(x + hstep, y - hstep);
				const auto &ld = sample(x - hstep, y + hstep);
				const auto &rd = sample(x + hstep, y + hstep);

				Point &p = mutPoint(x, y);
				assert(std::isnan(p.height));
				p.height = (lu.height + ru.height + ld.height + rd.height) * 0.25f
					+ (std::generate_canonical<float, 24>(rng) * 2.0f - 1.0f) * magnitude;
			}
		}

		// Square step, x-shifted
		for (int32_t y = 0; y < m_height; y += step) {
			for (int32_t x = hstep; x < m_width; x += step) {
				const auto &l = sample(x - hstep, y);
				const auto &r = sample(x + hstep, y);
				const auto &u = sample(x, y - hstep);
				const auto &d = sample(x, y + hstep);

				Point &p = mutPoint(x, y);
				assert(std::isnan(p.height));
				p.height = (l.height + r.height + u.height + d.height) * 0.25f
					+ (std::generate_canonical<float, 24>(rng) * 2.0f - 1.0f) * magnitude;
			}
		}

		// Square step, y-shifted
		for (int32_t y = hstep; y < m_height; y += step) {
			for (int32_t x = 0; x < m_width; x += step) {
				const auto &l = sample(x - hstep, y);
				const auto &r = sample(x + hstep, y);
				const auto &u = sample(x, y - hstep);
				const auto &d = sample(x, y + hstep);

				Point &p = mutPoint(x, y);
				assert(std::isnan(p.height));
				p.height = (l.height + r.height + u.height + d.height) * 0.25f
					+ (std::generate_canonical<float, 24>(rng) * 2.0f - 1.0f) * magnitude;
			}
		}

		step /= 2;
		magnitude *= 0.4f;
	}
}

const TerrainL0Map::Point &TerrainL0Map::point(int32_t x, int32_t y) const noexcept
{
	assert(x >= 0 && x < m_width);
	assert(y >= 0 && y < m_height);
	return m_points[size_t(y * m_width + x)];
}

std::shared_ptr<const TerrainL1Map> TerrainL0Map::l1map(int32_t x, int32_t y)
{
	assert(x >= 0 && x < m_width);
	assert(y >= 0 && y < m_height);

	Point &p = mutPoint(x, y);

	auto ptr = p.l1map.lock();
	if (ptr) {
		return ptr;
	}

	std::lock_guard lock(p.futex);

	if (ptr = p.l1map.lock(); ptr) {
		return ptr;
	}

	ptr = std::make_shared<TerrainL1Map>(257, 257); // (513, 513)

	double cx = (x + 0.5) * (L0_WIDTH / double(m_width)) - L0_WIDTH * 0.5;
	double cy = (y + 0.5) * (L0_HEIGHT / double(m_height)) - L0_HEIGHT * 0.5;
	double wx = L0_WIDTH * 2.2 / double(m_width);
	double wy = L0_HEIGHT * 2.2 / double(m_height);
	ptr->init(*this, p.l1seed, cx, cy, wx, wy);

	m_cache_ring[m_cache_ring_idx] = ptr;
	m_cache_ring_idx = (m_cache_ring_idx + 1u) % m_cache_ring.size();

	// TODO (Svenny): there is a race between this line and locking attempt before the futex
	p.l1map = ptr;
	return ptr;
}

double TerrainL0Map::sampleHeight(double x, double y) noexcept
{
	double wx = x;
	double wy = y;

	x = (x + L0_WIDTH * 0.5) * (double(m_width) / L0_WIDTH) - 0.5;
	y = (y + L0_HEIGHT * 0.5) * (double(m_height) / L0_HEIGHT) - 0.5;

	x = std::fmod(x, double(m_width));
	y = std::fmod(y, double(m_height));

	double xf = std::floor(x);
	double yf = std::floor(y);

	auto x0 = static_cast<int32_t>(xf);
	auto y0 = static_cast<int32_t>(yf);

	int32_t x1 = x0 + 1;
	int32_t y1 = y0 + 1;

	auto wrap = [](int32_t v, int32_t lim) {
		if (v < 0) {
			v += lim;
		}
		if (v >= lim) {
			v -= lim;
		}
		return v;
	};

	x0 = wrap(x0, m_width);
	y0 = wrap(y0, m_height);
	x1 = wrap(x1, m_width);
	y1 = wrap(y1, m_height);

	double tx = x - xf;
	double ty = y - yf;

	double lu = l1map(x0, y0)->sampleHeight(wx, wy);
	double ru = l1map(x1, y0)->sampleHeight(wx, wy);
	double ld = l1map(x0, y1)->sampleHeight(wx, wy);
	double rd = l1map(x1, y1)->sampleHeight(wx, wy);

	double hy0 = glm::mix(lu, ru, tx);
	double hy1 = glm::mix(ld, rd, tx);
	return glm::mix(hy0, hy1, ty);
}

float TerrainL0Map::sampleSelfHeight(double x, double y) const noexcept
{
	x = (x + L0_WIDTH * 0.5) * (double(m_width) / L0_WIDTH) - 0.5;
	y = (y + L0_HEIGHT * 0.5) * (double(m_height) / L0_HEIGHT) - 0.5;

	x = std::fmod(x, double(m_width));
	y = std::fmod(y, double(m_height));

	double xf = std::floor(x);
	double yf = std::floor(y);

	auto x0 = static_cast<int32_t>(xf);
	auto y0 = static_cast<int32_t>(yf);

	int32_t x1 = x0 + 1;
	int32_t y1 = y0 + 1;

	float tx = static_cast<float>(x - xf);
	float ty = static_cast<float>(y - yf);

	const Point &lu = m_points[pointIdxWrapped(x0, y0)];
	const Point &ru = m_points[pointIdxWrapped(x1, y0)];
	const Point &ld = m_points[pointIdxWrapped(x0, y1)];
	const Point &rd = m_points[pointIdxWrapped(x1, y1)];

	float hy0 = glm::mix(lu.height, ru.height, tx);
	float hy1 = glm::mix(ld.height, rd.height, tx);
	float res = glm::mix(hy0, hy1, ty);
	assert(std::isfinite(res));
	return res;
}

TerrainL0Map::Point &TerrainL0Map::mutPoint(int32_t x, int32_t y) noexcept
{
	return m_points[size_t(y * m_width + x)];
}

size_t TerrainL0Map::numPoints() const noexcept
{
	return size_t(m_width * m_height);
}

size_t TerrainL0Map::pointIdxWrapped(int32_t x, int32_t y) const noexcept
{
	if (x < 0) {
		x += m_width;
	} else if (x >= m_width) {
		x -= m_width;
	}

	if (y < 0) {
		y += m_height;
	} else if (y >= m_height) {
		y -= m_height;
	}

	return size_t(y * m_width + x);
}

TerrainGenerator::TerrainGenerator() : m_l0map(1536, 512)
{
	std::mt19937 rng(0xDEAD0004);
	m_l0map.init(rng);
}

void TerrainGenerator::generate(land::ChunkKey id, ChunkPrimaryData &output)
{
	g_map = &m_l0map;

	constexpr uint32_t GRID_SIZE = VoxelGrid::GRID_SIZE;
	using coord_t = HermiteDataEntry::coord_t;
	using ValuesArray = std::array<std::array<std::array<double, GRID_SIZE>, GRID_SIZE>, GRID_SIZE>;

	const int64_t base_x = id.x * int64_t(Config::CHUNK_SIZE);
	const int64_t base_y = id.y * int64_t(Config::CHUNK_SIZE);
	const int64_t base_z = id.z * int64_t(Config::CHUNK_SIZE);
	const uint32_t scale = (1u << id.scale_log2);

	// TODO: this is a temporary stub, add real land generator

	auto &grid = output.voxel_grid;
	// Temporary storage for SDF values, we will need it to find zero crossings
	auto p_values = std::make_unique<ValuesArray>();
	ValuesArray &values = *p_values;

	// Fill out values and voxels arrays
	for (uint32_t i = 0; i < GRID_SIZE; i++) {
		auto y = double(base_y + i * scale);
		for (uint32_t j = 0; j < GRID_SIZE; j++) {
			auto x = double(base_x + j * scale);
			voxel_t *scanline = grid.zScanline(j, i).data();

			for (uint32_t k = 0; k < GRID_SIZE; k++) {
				auto z = double(base_z + k * scale);

				double value = f(x, y, z);
				values[i][j][k] = value;

				if (value > 0.0) {
					scanline[k] = 0;
					continue;
				}

				auto mat = Material::Ground;

				if (y < 0.0) {
					mat = Material::Rock;
				}
				/*if (x < -2500.0 && z < -2500.0) {
					mat = Material::Grass;
				} else if (x < -2500.0 && z > 2500.0) {
					mat = Material::Rock;
				} else if (x > 2500.0 && z < -2500.0) {
					mat = Material::Snow;
				} else if (x > 2500.0 && z > 2500.0) {
					mat = Material::Sand;
				}

				double yn = y;
				if (yn > 500.0) {
					mat = Material::WolframOre;
				} else if (yn > 400.0) {
					mat = Material::UraniumOre;
				} else if (yn > 300.0) {
					mat = Material::GoldOre;
				} else if (yn > 200.0) {
					mat = Material::Coal;
				} else if (yn > 100.0) {
					mat = Material::IronOre;
				}*/

				scanline[k] = voxel_t(mat);
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
						addZeroCrossing<1>(ctx, f, df, output.hermite_data);
					}
				}
				if (j + 1 < GRID_SIZE) {
					ctx.sign_bigger = (values[i][j + 1][k] <= 0.0);
					if (ctx.sign_lesser != ctx.sign_bigger) {
						ctx.value_bigger = values[i][j + 1][k];
						ctx.voxel_bigger = voxels[i][j + 1][k];
						addZeroCrossing<0>(ctx, f, df, output.hermite_data);
					}
				}
				if (k + 1 < GRID_SIZE) {
					ctx.sign_bigger = (values[i][j][k + 1] <= 0.0);
					if (ctx.sign_lesser != ctx.sign_bigger) {
						ctx.value_bigger = values[i][j][k + 1];
						ctx.voxel_bigger = voxels[i][j][k + 1];
						addZeroCrossing<2>(ctx, f, df, output.hermite_data);
					}
				}
			}
		}
	}
}

} // namespace voxen::terrain
