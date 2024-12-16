#include "land_geometry_utils_private.hpp"

#include <glm/gtc/packing.hpp>

#include <algorithm>

namespace voxen::land::detail
{

namespace
{

uint16_t addSaturate(uint16_t a, uint16_t b) noexcept
{
	uint32_t x = static_cast<uint32_t>(a) + static_cast<uint32_t>(b);
	return static_cast<uint16_t>(std::min<uint32_t>(x, UINT16_MAX));
}

} // namespace

void GeometryUtils::addMatHistEntry(std::vector<SurfaceMatHistEntry> &entries, const SurfaceMatHistEntry &entry)
{
	for (auto &e : entries) {
		if (e.mat_id_or_color == entry.mat_id_or_color) {
			e.weight = addSaturate(e.weight, entry.weight);
			return;
		}
	}

	entries.emplace_back(entry);
}

void GeometryUtils::addMatHistEntry(std::vector<SurfaceMatHistEntry> &entries, const PseudoChunkData::CellEntry &cell)
{
	SurfaceMatHistEntry e[4];
	unpackCellEntryMatHist(e, cell);

	addMatHistEntry(entries, e[0]);
	addMatHistEntry(entries, e[1]);
	addMatHistEntry(entries, e[2]);
	addMatHistEntry(entries, e[3]);
}

void GeometryUtils::resolveMatHist(std::span<SurfaceMatHistEntry> entries, PseudoChunkData::CellEntry &cell)
{
	// Sort by weight decreasing
	std::sort(entries.begin(), entries.end(),
		[](const SurfaceMatHistEntry &a, const SurfaceMatHistEntry &b) { return a.weight > b.weight; });

	// Limit to the size of output storage
	const size_t num_entries = std::min<size_t>(entries.size(), 4);

	float weight_sum = 0.0f;
	for (size_t i = 0; i < num_entries; i++) {
		weight_sum += entries[i].weight;
	}

	cell.mat_hist_entries = {};
	cell.mat_hist_weights = {};

	for (size_t i = 0; i < num_entries; i++) {
		int ii = static_cast<int>(i);
		cell.mat_hist_entries[ii] = entries[i].mat_id_or_color;
		cell.mat_hist_weights[ii] = glm::packUnorm1x8(entries[i].weight / weight_sum);
	}
}

void GeometryUtils::unpackCellEntryMatHist(std::span<SurfaceMatHistEntry, 4> entries,
	const PseudoChunkData::CellEntry &cell) noexcept
{
	entries[0].mat_id_or_color = cell.mat_hist_entries.x;
	entries[0].weight = cell.mat_hist_weights.x;
	entries[1].mat_id_or_color = cell.mat_hist_entries.y;
	entries[1].weight = cell.mat_hist_weights.y;
	entries[2].mat_id_or_color = cell.mat_hist_entries.z;
	entries[2].weight = cell.mat_hist_weights.z;
	entries[3].mat_id_or_color = cell.mat_hist_entries.w;
	entries[3].weight = cell.mat_hist_weights.w;
}

} // namespace voxen::land::detail
