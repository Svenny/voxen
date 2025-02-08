#include <voxen/land/land_generator.hpp>

#include <voxen/land/land_public_consts.hpp>
#include <voxen/land/land_temp_blocks.hpp>
#include <voxen/land/land_utils.hpp>
#include <voxen/svc/task_builder.hpp>
#include <voxen/util/hash.hpp>

#include "land_geometry_utils_private.hpp"

#include <glm/gtc/packing.hpp>

#include <pcg/pcg_random.hpp>

#include <array>
#include <random>

namespace voxen::land
{

namespace
{

constexpr uint64_t DEFAULT_SEED = 0x42'6f'72'47'6f'41'63'6b;

constexpr int32_t WORLD_SIZE_X_CHUNKS = Consts::MAX_UNIQUE_WORLD_X_CHUNK + 1 - Consts::MIN_UNIQUE_WORLD_X_CHUNK;
constexpr int32_t WORLD_SIZE_Z_CHUNKS = Consts::MAX_UNIQUE_WORLD_Z_CHUNK + 1 - Consts::MIN_UNIQUE_WORLD_Z_CHUNK;

constexpr double WORLD_SIZE_X_METRES = Consts::BLOCK_SIZE_METRES * Consts::CHUNK_SIZE_BLOCKS * WORLD_SIZE_X_CHUNKS;
constexpr double WORLD_SIZE_Z_METRES = Consts::BLOCK_SIZE_METRES * Consts::CHUNK_SIZE_BLOCKS * WORLD_SIZE_Z_CHUNKS;

constexpr int32_t GLOBAL_MAP_ASPECT_RATIO = WORLD_SIZE_X_CHUNKS / WORLD_SIZE_Z_CHUNKS;

constexpr int32_t GLOBAL_MAP_HEIGHT = 1024;
constexpr int32_t GLOBAL_MAP_WIDTH = GLOBAL_MAP_HEIGHT * GLOBAL_MAP_ASPECT_RATIO;

constexpr int32_t GLOBAL_MAP_INIT_GRID_STEP = 16;
static_assert(GLOBAL_MAP_WIDTH % GLOBAL_MAP_INIT_GRID_STEP == 0);
static_assert(GLOBAL_MAP_HEIGHT % GLOBAL_MAP_INIT_GRID_STEP == 0);

constexpr float WATER_LEVEL_METRES = -1.5f;
constexpr float SHORE_LEVEL_METRES = 3.5f;
constexpr float MOUNTAIN_LEVEL_METRES = 750.0f;
constexpr float SNOW_PEAK_LEVEL_METRES = 2000.0f;

struct LocalPlaneSample {
	float global_map_height;
	float global_map_temperature;
	float surface_height;
};

glm::bvec2 fillLocalPlaneSample(GeneratorGlobalMap::SampledPoint sampled, LocalPlaneSample &output, float ymin,
	float ymax)
{
	output.global_map_height = sampled.height;
	output.global_map_temperature = sampled.temperature;
	output.surface_height = std::max(WATER_LEVEL_METRES, sampled.height);

	bool have_empty = ymax > output.surface_height;
	bool have_solid = ymin <= output.surface_height;
	return glm::bvec2(have_empty, have_solid);
}

Chunk::BlockId assignMaterial(LocalPlaneSample &sample, float y_height, bool true_chunk)
{
	if (y_height <= sample.surface_height) {
		if (y_height > sample.global_map_height) {
			// Under surface but above the global heightmap - filled by water
			return TempBlockMeta::BlockWater;
		}

		if (true_chunk) {
			// TODO: model something under the surface (stone, dirt)
		}

		if (y_height <= SHORE_LEVEL_METRES) {
			return TempBlockMeta::BlockSand;
		} else if (y_height <= MOUNTAIN_LEVEL_METRES) {
			return TempBlockMeta::BlockGrass;
		} else if (y_height <= SNOW_PEAK_LEVEL_METRES) {
			return TempBlockMeta::BlockStone;
		} else {
			return TempBlockMeta::BlockSnow;
		}
	}

	return TempBlockMeta::BlockEmpty;
}

glm::vec2 grad(int32_t x, int32_t z) noexcept
{
	uint64_t kek = Hash::xxh64Fixed((uint64_t(x) << 32) | uint64_t(z));

	uint32_t k1 = uint32_t(kek >> 32);
	uint32_t k2 = uint32_t(kek);

	constexpr uint32_t S = 1u << 31;
	constexpr uint32_t M = 16777215u;
	float gx = ((k1 & S) ? -float(k1 & M) : float(k1 & M)) / float(M);
	float gy = ((k2 & S) ? -float(k2 & M) : float(k2 & M)) / float(M);
	return glm::vec2(gx, gy);
}

float sampleRawSimplexNoise(double x, double z)
{
	constexpr double F = 0.3660254038;
	constexpr double G = 0.2113248654;

	double xskew = x + (x + z) * F;
	double zskew = z + (x + z) * F;

	double x0d = std::floor(xskew);
	double z0d = std::floor(zskew);
	int32_t x0 = static_cast<int32_t>(x0d);
	int32_t z0 = static_cast<int32_t>(z0d);

	glm::vec2 inner(xskew - x0d, zskew - z0d);

	int32_t x1 = inner.x >= inner.y ? x0 + 1 : x0;
	int32_t z1 = inner.x < inner.y ? z0 + 1 : z0;
	double x1d = double(x1);
	double z1d = double(z1);

	int32_t x2 = x0 + 1;
	int32_t z2 = z0 + 1;
	double x2d = double(x2);
	double z2d = double(z2);

	double x0_unskew = x0d - (x0d + z0d) * G;
	double z0_unskew = z0d - (x0d + z0d) * G;
	double x1_unskew = x1d - (x1d + z1d) * G;
	double z1_unskew = z1d - (x1d + z1d) * G;
	double x2_unskew = x2d - (x2d + z2d) * G;
	double z2_unskew = z2d - (x2d + z2d) * G;

	glm::vec2 r0(x - x0_unskew, z - z0_unskew);
	glm::vec2 r1(x - x1_unskew, z - z1_unskew);
	glm::vec2 r2(x - x2_unskew, z - z2_unskew);

	float d0 = std::max(0.0f, 0.5f - r0.x * r0.x - r0.y * r0.y);
	float d1 = std::max(0.0f, 0.5f - r1.x * r1.x - r1.y * r1.y);
	float d2 = std::max(0.0f, 0.5f - r2.x * r2.x - r2.y * r2.y);

	d0 = d0 * d0;
	d0 = d0 * d0;
	d1 = d1 * d1;
	d1 = d1 * d1;
	d2 = d2 * d2;
	d2 = d2 * d2;

	float g0 = glm::dot(grad(x0, z0), r0);
	float g1 = glm::dot(grad(x1, z1), r1);
	float g2 = glm::dot(grad(x2, z2), r2);

	float result = d0 * g0 + d1 * g1 + d2 * g2;
	// TODO: why this multiplication?
	return 16.0f * result;
}

float sampleOctavedRawSimplexNoise(double x, double z)
{
	float noise = 0.0f;

	noise += 450.0f * sampleRawSimplexNoise(x * 0.001, z * 0.001);
	noise += 250.0f * sampleRawSimplexNoise(x * 0.002, z * 0.002);
	noise += 150.0f * sampleRawSimplexNoise(x * 0.004, z * 0.004);
	noise += 50.0f * sampleRawSimplexNoise(x * 0.01, z * 0.01);
	noise += 25.0f * sampleRawSimplexNoise(x * 0.025, z * 0.025);
	noise += 13.0f * sampleRawSimplexNoise(x * 0.05, z * 0.05);
	noise += 4.5f * sampleRawSimplexNoise(x * 0.1, z * 0.1);
	noise += 1.5f * sampleRawSimplexNoise(x * 0.3, z * 0.3);

	return noise;
}

float sampleOctavedWrappedSimplexNoise(double x, double z)
{
	// This will bring coordinates in range [-N/2:N/2]. Not sure which ends
	// are inclusive/exclusive but that doesn't matter much.
	// Combining samples of four pairs (x; z) (x; -z) (-x; z) (-x; -z)
	// will make any noise function correctly tile at world boundaries:
	// other "aliased" positions will sample from same set of points.
	//
	// TODO: but is taking 4x noise samples and severely messing up
	// its value distribution in the process worth it?
	// One of the better solutions would be to move this hack to the
	// outermost sampling procedure and introduce logic to skip sampling
	// an axis twice if it's far from the wrapparound point (close to zero),
	// smoothly introducing the second sample as we're getting closer to it.
	x = fmod(x, 0.5 * WORLD_SIZE_X_METRES);
	z = fmod(z, 0.5 * WORLD_SIZE_Z_METRES);

	float samples[4] = { sampleOctavedRawSimplexNoise(x, z), sampleOctavedRawSimplexNoise(-x, -z),
		sampleOctavedRawSimplexNoise(x, -z), sampleOctavedRawSimplexNoise(-x, z) };
	// Sort it manually (and partially)
	if (samples[0] > samples[1]) {
		std::swap(samples[0], samples[1]);
	}
	if (samples[1] > samples[2]) {
		std::swap(samples[1], samples[2]);
	}
	if (samples[2] > samples[3]) {
		std::swap(samples[2], samples[3]);
	}
	// Now `samples[3]` is the largest
	if (samples[0] > samples[1]) {
		std::swap(samples[0], samples[1]);
	}
	if (samples[1] > samples[2]) {
		std::swap(samples[1], samples[2]);
	}
	// Now `samples[2]` is the second largest

	// Kinda softmax
	return 0.125f * samples[0] + 0.125f * samples[1] + 0.25f * samples[2] + 0.5f * samples[3];
}

} // namespace

uint64_t GeneratorGlobalMap::enqueueGenerate(uint64_t seed, svc::TaskBuilder &bld)
{
	bld.enqueueTask([this, seed](svc::TaskContext &) { this->doGenerate(seed); });
	return bld.getLastTaskCounter();
}

void GeneratorGlobalMap::doGenerate(uint64_t seed)
{
	constexpr float HEIGHT_LEVEL_POINTS[] = { -1500.0f, -500.0f, -100.0f, 75.0f, 500.0f, 2500.0f, 5000.f };
	constexpr float HEIGHT_LEVEL_WEIGHTS[] = { 1.0f, 10.0f, 20.0f, 35.0f, 20.0f, 7.5f, 1.0f };

	constexpr float HEIGHT_NOISE_WIDTH = 1500.0f;

	constexpr float MAX_BASE_TEMPERATURE = 30.0f;
	constexpr float MIN_BASE_TEMPERATURE = -15.0f;
	constexpr float TEMPERATURE_NOISE_WIDTH = 20.0f;

	constexpr float NOISE_MAGNITUDE_MULTIPLIER = 0.4f;

	int32_t cur_width = GLOBAL_MAP_WIDTH / GLOBAL_MAP_INIT_GRID_STEP;
	int32_t cur_height = GLOBAL_MAP_HEIGHT / GLOBAL_MAP_INIT_GRID_STEP;

	// Initialize with random values generated at low resolution grid,
	// then progressively "upscale" it using diamond-square algorithm.
	pcg64_oneseq rng(seed);
	std::piecewise_linear_distribution<float> height_level_dist(std::begin(HEIGHT_LEVEL_POINTS),
		std::end(HEIGHT_LEVEL_POINTS), std::begin(HEIGHT_LEVEL_WEIGHTS));
	std::uniform_real_distribution<float> base_temperature_dist(MIN_BASE_TEMPERATURE, MAX_BASE_TEMPERATURE);

	std::vector<glm::vec2> cur_image(size_t(cur_width * cur_height));
	for (glm::vec2 &v : cur_image) {
		float height_level = height_level_dist(rng);
		float base_temperature = 0.5f * (base_temperature_dist(rng) + base_temperature_dist(rng));

		v.x = height_level;
		v.y = base_temperature;
	}

	int32_t prev_width = cur_width;
	int32_t prev_height = cur_height;
	std::vector<glm::vec2> prev_image = std::move(cur_image);

	auto load_prev_image = [&](int32_t row, int32_t col) -> glm::vec2 {
		row = (row + prev_height) % prev_height;
		col = (col + prev_width) % prev_width;
		return prev_image[size_t(row * prev_width + col)];
	};

	auto load_cur_image = [&](int32_t row, int32_t col) -> glm::vec2 {
		row = (row + cur_height) % cur_height;
		col = (col + cur_width) % cur_width;
		return cur_image[size_t(row * cur_width + col)];
	};

	auto store_cur_image = [&](int32_t row, int32_t col, glm::vec2 value) {
		cur_image[size_t(row * cur_width + col)] = value;
	};

	auto average_four = [](const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c, const glm::vec2 &d) -> glm::vec2 {
		// Weight elements inversely by height to shift the average towards zero.
		// This should make elevation features more sharply pronounced.
		float heights[4] = { a.x, b.x, c.x, d.x };
		std::sort(heights, heights + 4);

		const float height = 0.35f * heights[0] + 0.3f * heights[1] + 0.2f * heights[2] + 0.15f * heights[3];
		const float temperature = 0.25f * (a.y + b.y + c.y + d.y);
		return glm::vec2(height, temperature);
	};

	float noise_magnitude = 1.0f;

	// Create a new image at each step, otherwise the first few
	// iterations will index the image too sparsely, bad for caches
	while (cur_height != GLOBAL_MAP_HEIGHT) {
		cur_width = prev_width * 2;
		cur_height = prev_height * 2;
		cur_image = std::vector<glm::vec2>(size_t(cur_width * cur_height));

		std::uniform_real_distribution<float> height_noise_dist(-HEIGHT_NOISE_WIDTH * noise_magnitude,
			HEIGHT_NOISE_WIDTH * noise_magnitude);
		std::uniform_real_distribution<float> temperature_noise_dist(-TEMPERATURE_NOISE_WIDTH * noise_magnitude,
			TEMPERATURE_NOISE_WIDTH * noise_magnitude);

		// Diamond step - copy/interpolate values in checkerboard pattern
		for (int32_t row = 0; row < cur_height; row += 2) {
			for (int32_t col = 0; col < cur_width; col += 2) {
				// 2x2 current resolution block looks like this:
				//
				// UL XX | UL XX | ...
				// XX DM | XX DM | ...
				// ------+-------+-
				// UL XX | UL XX | ...
				// XX DM | XX DM | ...
				// ------+-------+-
				// .. .. | .. .. |
				//
				// - UL is copied upper-left (1 pixel from this location at previous resolution)
				// - DM is diamond-averaged from four adjacent ULs
				// - XX are missed for now and left for square step
				glm::vec2 upper_left = load_prev_image(row / 2, col / 2);
				glm::vec2 upper_right = load_prev_image(row / 2, col / 2 + 1);
				glm::vec2 lower_left = load_prev_image(row / 2 + 1, col / 2);
				glm::vec2 lower_right = load_prev_image(row / 2 + 1, col / 2 + 1);

				glm::vec2 dm = average_four(upper_left, upper_right, lower_left, lower_right);
				dm.x += height_noise_dist(rng);
				dm.y += temperature_noise_dist(rng);

				store_cur_image(row, col, upper_left);
				store_cur_image(row + 1, col + 1, dm);
			}
		}

		// Square step - fill remaining values (checkerboard "holes")
		for (int32_t row = 0; row < cur_height; row += 2) {
			for (int32_t col = 0; col < cur_width; col += 2) {
				// 2x2 current resolution block looks like this:
				//
				//    | .. DM |
				//   -+-------+-
				// .. | UL AA | UL
				// DM | BB DM | ..
				//   -+-------+-
				//    | UL .. |
				//
				// - UL/DM are filled in the diamond step
				// - AA/BB will be square-averaged from four adjacent UL/DMs

				// row - 1
				glm::vec2 dm_up = load_cur_image(row - 1, col + 1);
				// row
				glm::vec2 ul_this = load_cur_image(row, col);
				glm::vec2 ul_right = load_cur_image(row, col + 2);
				// row + 1
				glm::vec2 dm_left = load_cur_image(row + 1, col - 1);
				glm::vec2 dm_this = load_cur_image(row + 1, col + 1);
				// row + 2
				glm::vec2 ul_down = load_cur_image(row + 2, col);

				glm::vec2 aa = average_four(dm_up, ul_this, ul_right, dm_this);
				glm::vec2 bb = average_four(ul_this, dm_left, dm_this, ul_down);

				aa.x += height_noise_dist(rng);
				aa.y += temperature_noise_dist(rng);

				bb.x += height_noise_dist(rng);
				bb.y += temperature_noise_dist(rng);

				store_cur_image(row, col + 1, aa);
				store_cur_image(row + 1, col, bb);
			}
		}

		prev_width = cur_width;
		prev_height = cur_height;
		prev_image = std::move(cur_image);

		noise_magnitude *= NOISE_MAGNITUDE_MULTIPLIER;
	}

	const size_t num_points = size_t(cur_width * cur_height);

	m_width = cur_width;
	m_height = cur_height;
	m_points = std::make_unique<Point[]>(num_points);

	for (size_t i = 0; i < num_points; i++) {
		const glm::vec2 &in = prev_image[i];
		Point &out = m_points[i];

		out.height = static_cast<int16_t>(in.x);
		out.temperature = static_cast<int8_t>(in.y);
		out.variance = 0;
	}
}

auto GeneratorGlobalMap::sample(double x, double z) const noexcept -> SampledPoint
{
	double sample_x = m_width * (x + WORLD_SIZE_X_METRES * 0.5) / WORLD_SIZE_X_METRES;
	double sample_z = m_height * (z + WORLD_SIZE_Z_METRES * 0.5) / WORLD_SIZE_Z_METRES;

	double xf = std::floor(sample_x);
	double zf = std::floor(sample_z);

	auto x0 = static_cast<int32_t>(xf);
	auto z0 = static_cast<int32_t>(zf);

	int32_t x1 = x0 + 1;
	int32_t z1 = z0 + 1;

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
	z0 = wrap(z0, m_height);
	x1 = wrap(x1, m_width);
	z1 = wrap(z1, m_height);

	float tx = static_cast<float>(sample_x - xf);
	float tz = static_cast<float>(sample_z - zf);

	const Point &lu = m_points[size_t(z0 * m_width + x0)];
	const Point &ru = m_points[size_t(z0 * m_width + x1)];
	const Point &ld = m_points[size_t(z1 * m_width + x0)];
	const Point &rd = m_points[size_t(z1 * m_width + x1)];

	float hz0 = glm::mix(float(lu.height), float(ru.height), tx);
	float tz0 = glm::mix(float(lu.temperature), float(ru.temperature), tx);

	float hz1 = glm::mix(float(ld.height), float(rd.height), tx);
	float tz1 = glm::mix(float(ld.temperature), float(rd.temperature), tx);

	float hres = glm::mix(hz0, hz1, tz);
	float tres = glm::mix(tz0, tz1, tz);
	assert(std::isfinite(hres));
	assert(std::isfinite(tres));

	return SampledPoint {
		.height = hres,
		.temperature = tres,
	};
}

Generator::Generator()
{
	setSeed(DEFAULT_SEED);
}

Generator::~Generator() = default;

void Generator::onWorldTickBegin(world::TickId new_tick)
{
	m_current_world_tick = new_tick;
}

void Generator::setSeed(uint64_t seed)
{
	m_initial_seed = seed;

	m_global_map_sub_seed = Hash::xxh64Fixed(seed ^ 10'25'1337'10001);
	m_regional_map_sub_seed = Hash::xxh64Fixed(seed ^ 10'25'1337'10002);
	m_local_noise_sub_seed = Hash::xxh64Fixed(seed ^ 10'25'1337'10003);

	// TODO: force regeneration of global/regional maps
}

void Generator::waitEnqueuedTasks(svc::TaskBuilder &bld)
{
	bld.addWait(m_global_map_gen_task_counter);
	bld.enqueueSyncPoint().wait();
}

uint64_t Generator::prepareKeyGeneration(ChunkKey key, svc::TaskBuilder &bld)
{
	// TODO: fire regional map generation task for `key`
	(void) key;
	return ensureGlobalMap(bld);
}

void Generator::generateChunk(ChunkKey key, Chunk &output)
{
	const glm::ivec3 min_blockspace = key.base() * Consts::CHUNK_SIZE_BLOCKS;
	// Sample points are shifted by 0.5 to be in centers of block volumes
	const glm::dvec3 min_world = (glm::dvec3(min_blockspace) + 0.5) * Consts::BLOCK_SIZE_METRES;

	float y_height[Consts::CHUNK_SIZE_BLOCKS];
	for (int32_t y = 0; y < Consts::CHUNK_SIZE_BLOCKS; y++) {
		y_height[y] = static_cast<float>(double(min_blockspace.y + y) * Consts::BLOCK_SIZE_METRES);
	}

	const float ymin = y_height[0];
	const float ymax = y_height[Consts::CHUNK_SIZE_BLOCKS - 1];

	using LocalPlane = std::array<std::array<LocalPlaneSample, Consts::CHUNK_SIZE_BLOCKS>, Consts::CHUNK_SIZE_BLOCKS>;
	auto local_plane = std::make_unique<LocalPlane>();

	glm::bvec2 have_empty_solid(false, false);

	for (uint32_t x = 0; x < Consts::CHUNK_SIZE_BLOCKS; x++) {
		for (uint32_t z = 0; z < Consts::CHUNK_SIZE_BLOCKS; z++) {
			double sample_x = min_world.x + x * Consts::BLOCK_SIZE_METRES;
			double sample_z = min_world.z + z * Consts::BLOCK_SIZE_METRES;

			GeneratorGlobalMap::SampledPoint sp = m_global_map.sample(sample_x, sample_z);
			sp.height += sampleOctavedWrappedSimplexNoise(sample_x, sample_z);
			have_empty_solid |= fillLocalPlaneSample(sp, (*local_plane)[x][z], ymin, ymax);
		}
	}

	if (!have_empty_solid.y) {
		// No solid blocks
		output.setAllBlocksUniform(TempBlockMeta::BlockEmpty);
		return;
	}

	// Allocate on heap, expanded array is pretty large
	auto ids = std::make_unique<Chunk::BlockIdArray>();

	Utils::forYXZ<Consts::CHUNK_SIZE_BLOCKS>([&](uint32_t x, uint32_t y, uint32_t z) {
		ids->store(x, y, z, assignMaterial((*local_plane)[x][z], y_height[y], true));
	});

	output.setAllBlocks(ids->cview());
}

void Generator::generatePseudoChunk(ChunkKey key, PseudoChunkData &output)
{
	const glm::ivec3 min_blockspace = key.base() * Consts::CHUNK_SIZE_BLOCKS;
	const int32_t step_blockspace = key.scaleMultiplier();

	float y_height[Consts::CHUNK_SIZE_BLOCKS + 1];
	for (int32_t y = 0; y <= Consts::CHUNK_SIZE_BLOCKS; y++) {
		y_height[y] = static_cast<float>(double(min_blockspace.y + y * step_blockspace) * Consts::BLOCK_SIZE_METRES);
	}

	const float ymin = y_height[0];
	const float ymax = y_height[Consts::CHUNK_SIZE_BLOCKS];

	constexpr int32_t NP = Consts::CHUNK_SIZE_BLOCKS + 1;

	using LocalPlane = std::array<std::array<LocalPlaneSample, NP>, NP>;
	auto local_plane = std::make_unique<LocalPlane>();

	glm::bvec2 have_empty_solid(false, false);

	for (int32_t x = 0; x <= Consts::CHUNK_SIZE_BLOCKS; x++) {
		for (int32_t z = 0; z <= Consts::CHUNK_SIZE_BLOCKS; z++) {
			double sample_x = double(min_blockspace.x + x * step_blockspace) * Consts::BLOCK_SIZE_METRES;
			double sample_z = double(min_blockspace.z + z * step_blockspace) * Consts::BLOCK_SIZE_METRES;

			GeneratorGlobalMap::SampledPoint sp = m_global_map.sample(sample_x, sample_z);
			sp.height += sampleOctavedWrappedSimplexNoise(sample_x, sample_z);
			have_empty_solid |= fillLocalPlaneSample(sp, (*local_plane)[size_t(x)][size_t(z)], ymin, ymax);
		}
	}

	if (!have_empty_solid.x || !have_empty_solid.y) {
		return;
	}

	std::vector<PseudoChunkData::CellEntry> cells;
	std::vector<detail::SurfaceMatHistEntry> material_histogram;

	Utils::forYXZ<Consts::CHUNK_SIZE_BLOCKS>([&](uint32_t x, uint32_t y, uint32_t z) {
		const float y0 = y_height[y];
		const float y1 = y_height[y + 1];

		const float h00 = (*local_plane)[x][z].surface_height;
		const float h01 = (*local_plane)[x][z + 1].surface_height;
		const float h10 = (*local_plane)[x + 1][z].surface_height;
		const float h11 = (*local_plane)[x + 1][z + 1].surface_height;

		float values[8];
		values[0] = y0 - h00;
		values[4] = y1 - h00;
		values[1] = y0 - h01;
		values[5] = y1 - h01;
		values[2] = y0 - h10;
		values[6] = y1 - h10;
		values[3] = y0 - h11;
		values[7] = y1 - h11;

		// Reset state from previous cell aggregation
		material_histogram.clear();

		PseudoChunkData::CellEntry cell {};
		cell.cell_index = glm::u8vec3(x, y, z);

		for (int i = 0; i < 8; i++) {
			if (values[i] <= 0.0f) {
				cell.corner_solid_mask |= (1 << i);

				auto &sample = (*local_plane)[x + ((i & 0b010) ? 1 : 0)][z + ((i & 0b001) ? 1 : 0)];
				Chunk::BlockId block_id = assignMaterial(sample, (i & 0b100) ? y1 : y0, false);
				uint16_t block_color = TempBlockMeta::packColor555(TempBlockMeta::BLOCK_FIXED_COLOR[block_id]);
				detail::GeometryUtils::addMatHistEntry(material_histogram, { block_color, 255 });
			}
		}

		if (cell.corner_solid_mask == 0 || cell.corner_solid_mask == 255) {
			// No surface intersections
			return;
		}

		// Calculate the average of surface intersections positions
		glm::vec3 surface_point_sum(0.0f);
		uint16_t surface_point_count = 0;

		for (int i = 0; i < 8; i++) {
			if (values[i] > 0.0f) {
				continue;
			}

			constexpr float NORM_DIV = 1.0f / float(Consts::CHUNK_SIZE_BLOCKS);
			const float edge_x_norm = float(int32_t(x) + ((i >> 1) & 1)) * NORM_DIV;
			const float edge_y_norm = float(int32_t(y) + ((i >> 2) & 1)) * NORM_DIV;
			const float edge_z_norm = float(int32_t(z) + ((i >> 0) & 1)) * NORM_DIV;

			// Given `y(0) = v0` and `y(1) = v1` where `v0` and `v1` have different signs,
			// finds X of zero crossing (between 0 and 1) using linear interpolation
			auto solve = [](float v0, float v1) -> float { return -v0 / (v1 - v0); };

			// This is a solid corner, find adjacent non-solid ones
			if (values[i ^ 0b010] > 0.0f) {
				// X edge, reverse offset if we are in "upper" end now
				float offset = ((i & 0b010) ? -NORM_DIV : NORM_DIV) * solve(values[i], values[i ^ 0b010]);

				surface_point_sum += glm::vec3(edge_x_norm + offset, edge_y_norm, edge_z_norm);
				surface_point_count++;
			}

			if (values[i ^ 0b100] > 0.0f) {
				// Y edge, reverse offset if we are in "upper" end now
				float offset = ((i & 0b100) ? -NORM_DIV : NORM_DIV) * solve(values[i], values[i ^ 0b100]);

				surface_point_sum += glm::vec3(edge_x_norm, edge_y_norm + offset, edge_z_norm);
				surface_point_count++;
			}

			if (values[i ^ 0b001] > 0.0f) {
				// Z edge, reverse offset if we are in "upper" end now
				float offset = ((i & 0b001) ? -NORM_DIV : NORM_DIV) * solve(values[i], values[i ^ 0b001]);

				surface_point_sum += glm::vec3(edge_x_norm, edge_y_norm, edge_z_norm + offset);
				surface_point_count++;
			}
		}

		detail::GeometryUtils::resolveMatHist(material_histogram, cell);
		cell.surface_point_unorm = glm::packUnorm<uint16_t>(surface_point_sum / float(surface_point_count));
		cell.surface_point_sum_count = surface_point_count;

		cells.emplace_back(cell);
	});

	output.generateExternally(cells);
}

uint64_t Generator::ensureGlobalMap(svc::TaskBuilder &bld)
{
	if (m_global_map_gen_task_counter > 0) [[likely]] {
		return m_global_map_gen_task_counter;
	}

	m_global_map_gen_task_counter = m_global_map.enqueueGenerate(m_global_map_sub_seed, bld);
	return m_global_map_gen_task_counter;
}

} // namespace voxen::land
