#include <voxen/common/assets/png_tools.hpp>
#include <voxen/common/filemanager.hpp>
#include <voxen/common/terrain/generator.hpp>
#include <voxen/util/log.hpp>

#include <extras/dyn_array.hpp>

#include <glm/common.hpp>
#include <glm/vec3.hpp>

#include <cassert>

using voxen::Log;
using voxen::terrain::TerrainL0Map;
using voxen::terrain::TerrainL1Map;

template<typename T>
static glm::vec3 heightToRgb(T h)
{
	const T SMOOTH_RANGE = 50.0;
	const T MIN_HEIGHT = -20'000.0;
	const T MAX_HEIGHT = 15'000.0;

	const glm::vec3 higround = glm::vec3(1.0, 1.0, 1.0);
	const glm::vec3 loground = glm::vec3(20.0 / 255.0, 120.0 / 255.0, 26.0 / 255.0);
	const glm::vec3 hiwater = glm::vec3(32.0 / 255.0, 64.0 / 255.0, 0.9);
	const glm::vec3 lowater = glm::vec3(16.0 / 255.0, 32.0 / 255.0, 0.5);

	h = glm::clamp(h, MIN_HEIGHT, MAX_HEIGHT);

	if (h >= SMOOTH_RANGE) {
		return glm::mix(loground, higround, h / MAX_HEIGHT);
	}

	if (h <= -SMOOTH_RANGE) {
		return glm::mix(hiwater, lowater, h / MIN_HEIGHT);
	}

	return glm::mix(hiwater, loground, (h + SMOOTH_RANGE) / (T(2.0) * SMOOTH_RANGE));
}

static void dumpL1Map(TerrainL0Map &l0map, int32_t xx, int32_t yy, std::string_view path)
{
	std::shared_ptr<const TerrainL1Map> l1map_ptr = l0map.l1map(xx, yy);
	const TerrainL1Map &l1map = *l1map_ptr;

	extras::dyn_array<uint8_t> image(size_t(l1map.width() * l1map.height() * 3));

	for (int32_t y = 0; y < l1map.height(); y++) {
		for (int32_t x = 0; x < l1map.width(); x++) {
			glm::vec3 rgb = heightToRgb(l1map.point(x, y).height);

			auto idx = size_t((y * l1map.width() + x) * 3);
			rgb = 255.0f * glm::clamp(rgb, 0.0f, 1.0f);
			image[idx + 0] = uint8_t(rgb.r);
			image[idx + 1] = uint8_t(rgb.g);
			image[idx + 2] = uint8_t(rgb.b);
		}
	}

	const voxen::assets::PngInfo png = { .resolution = { .width = l1map.width(), .height = l1map.height() },
		.is_16bpc = false,
		.channels = 3 };

	auto shit = voxen::assets::PngTools::pack(image.as_bytes(), png, false);
	[[maybe_unused]] bool val = voxen::FileManager::writeUserFile(path, shit.as_bytes());
	assert(val);
}

[[maybe_unused]] static void dumpL0Map(TerrainL0Map &l0map, int32_t width, int32_t height, std::string_view path)
{
	constexpr static double L0_WIDTH = 8.975530211306038e+7 / 1.0;
	constexpr static double L0_HEIGHT = 3.0745084822905876e+7 / 1.0;

	extras::dyn_array<double> values(size_t(width * height));

	std::atomic_size_t counter(0);

//#pragma omp parallel for
	for (size_t i = 0; i < values.size(); i++) {
		uint32_t x = i % uint32_t(width);
		uint32_t y = uint32_t(i / uint32_t(width));

		double u = (x + 0.5) / double(width);
		double v = (y + 0.5) / double(height);

		double wx = (u - 0.5) * L0_WIDTH;
		double wy = (v - 0.5) * L0_HEIGHT;

		double vv = l0map.sampleHeight(wx, wy);
		values[i] = vv;

		size_t done = 1 + counter.fetch_add(1, std::memory_order_relaxed);
		if ((done * 100) % values.size() == 0) {
			Log::warn("{}%", (done * 100) / values.size());
		}
	}

	extras::dyn_array<uint8_t> image(size_t(width * height * 3));

	for (size_t i = 0; i < values.size(); i++) {
		glm::vec3 rgb = heightToRgb(values[i]);

		rgb = 255.0f * glm::clamp(rgb, 0.0f, 1.0f);
		image[i * 3 + 0] = uint8_t(rgb.r);
		image[i * 3 + 1] = uint8_t(rgb.g);
		image[i * 3 + 2] = uint8_t(rgb.b);
	}

	const voxen::assets::PngInfo png = { .resolution = { .width = width, .height = height },
		.is_16bpc = false,
		.channels = 3 };

	auto shit = voxen::assets::PngTools::pack(image.as_bytes(), png, false);
	[[maybe_unused]] bool val = voxen::FileManager::writeUserFile(path, shit.as_bytes());
	assert(val);
}

int main(int /*argc*/, char * /*argv*/[])
{
	constexpr int32_t TMW = 512; //1536;
	constexpr int32_t TMH = 256; //512;

	TerrainL0Map mm(TMW, TMH);

	std::mt19937 rng(0xDEADBEEF);
	mm.init(rng);

	extras::dyn_array<uint8_t> image(TMW * TMH * 3);

	for (int32_t y = 0; y < TMH; y++) {
		for (int32_t x = 0; x < TMW; x++) {
			glm::vec3 rgb = heightToRgb(mm.point(x, y).height);

			auto idx = size_t((y * TMW + x) * 3);
			rgb = 255.0f * glm::clamp(rgb, 0.0f, 1.0f);
			image[idx + 0] = uint8_t(rgb.r);
			image[idx + 1] = uint8_t(rgb.g);
			image[idx + 2] = uint8_t(rgb.b);
		}
	}

	constexpr voxen::assets::PngInfo png = { .resolution = { .width = TMW, .height = TMH },
		.is_16bpc = false,
		.channels = 3 };

	auto path = fmt::format("/run/user/1000/voxen/hmap.png");
	auto shit = voxen::assets::PngTools::pack(image.as_bytes(), png, false);
	[[maybe_unused]] bool val = voxen::FileManager::writeUserFile(path, shit.as_bytes());
	assert(val);

	dumpL1Map(mm, 72, 28, "/run/user/1000/voxen/hmap-72-28.png");
	//dumpL1Map(mm, 823, 263, "/run/user/1000/voxen/hmap-823-263.png");
	//dumpL1Map(mm, 864, 256, "/run/user/1000/voxen/hmap-864-256.png");

	dumpL0Map(mm, TMW * 16, TMH * 16, "/run/user/1000/voxen/hmap-large.png");

	return 0;
}
