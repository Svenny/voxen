#pragma once

#include <voxen/common/terrain/chunk_id.hpp>
#include <voxen/os/futex.hpp>
#include <voxen/visibility.hpp>

#include <array>
#include <memory>
#include <random>

namespace voxen::terrain
{

struct ChunkPrimaryData;

class TerrainL0Map;

class VOXEN_API TerrainL1Map final {
public:
	struct Point {
		float height;
	};

	explicit TerrainL1Map(int32_t width, int32_t height);
	TerrainL1Map(TerrainL1Map &&) = delete;
	TerrainL1Map(const TerrainL1Map &) = delete;
	TerrainL1Map &operator=(TerrainL1Map &&) = delete;
	TerrainL1Map &operator=(const TerrainL1Map &) = delete;
	~TerrainL1Map() = default;

	void init(const TerrainL0Map &l0map, uint32_t seed, double center_x, double center_y, double world_size_x,
		double world_size_y);

	int32_t width() const noexcept { return m_width; }
	int32_t height() const noexcept { return m_height; }

	const Point &point(int32_t x, int32_t y) const noexcept;

	double sampleHeight(double x, double y) const noexcept;

private:
	int32_t m_width;
	int32_t m_height;
	double m_world_left_x;
	double m_world_upper_y;
	double m_world_size_x;
	double m_world_size_y;

	std::unique_ptr<Point[]> m_points;

	Point &mutPoint(int32_t x, int32_t y) noexcept;
	size_t numPoints() const noexcept;
};

class VOXEN_API TerrainL0Map final {
public:
	struct Point {
		float height;
		uint32_t l1seed;
		os::FutexLock futex;
		std::weak_ptr<TerrainL1Map> l1map;
	};

	explicit TerrainL0Map(int32_t width, int32_t height);
	TerrainL0Map(TerrainL0Map &&) = delete;
	TerrainL0Map(const TerrainL0Map &) = delete;
	TerrainL0Map &operator=(TerrainL0Map &&) = delete;
	TerrainL0Map &operator=(const TerrainL0Map &) = delete;
	~TerrainL0Map() = default;

	void init(std::mt19937 &rng);

	int32_t width() const noexcept { return m_width; }
	int32_t height() const noexcept { return m_height; }

	const Point &point(int32_t x, int32_t y) const noexcept;
	std::shared_ptr<const TerrainL1Map> l1map(int32_t x, int32_t y);

	double sampleHeight(double x, double y) noexcept;
	float sampleSelfHeight(double x, double y) const noexcept;

private:
	int32_t m_width;
	int32_t m_height;

	std::unique_ptr<Point[]> m_points;

	uint32_t m_cache_ring_idx = 0;
	std::array<std::shared_ptr<TerrainL1Map>, 256> m_cache_ring;

	Point &mutPoint(int32_t x, int32_t y) noexcept;
	size_t numPoints() const noexcept;
	size_t pointIdxWrapped(int32_t x, int32_t y) const noexcept;
};

class TerrainGenerator final {
public:
	TerrainGenerator();

	void generate(ChunkId id, ChunkPrimaryData &output);

private:
	TerrainL0Map m_l0map;
};

} // namespace voxen::terrain
