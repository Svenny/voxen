#pragma once

#include <voxen/land/pseudo_chunk_data.hpp>

#include <cstdint>
#include <vector>

namespace voxen::land::detail
{

struct SurfaceMatHistEntry {
	uint16_t mat_id_or_color = 0;
	uint16_t weight = 0;
};

namespace GeometryUtils
{

void addMatHistEntry(std::vector<SurfaceMatHistEntry> &entries, const SurfaceMatHistEntry &entry);
void addMatHistEntry(std::vector<SurfaceMatHistEntry> &entries, const PseudoChunkData::CellEntry &cell);

void resolveMatHist(std::span<SurfaceMatHistEntry> entries, PseudoChunkData::CellEntry &cell);

void unpackCellEntryMatHist(std::span<SurfaceMatHistEntry, 4> entries, const PseudoChunkData::CellEntry &cell) noexcept;

} // namespace GeometryUtils

} // namespace voxen::land::detail
