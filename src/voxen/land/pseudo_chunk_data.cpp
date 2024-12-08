#include <voxen/land/pseudo_chunk_data.hpp>

#include <voxen/land/land_chunk.hpp>
#include <voxen/land/land_public_consts.hpp>
#include <voxen/land/land_utils.hpp>
#include <voxen/util/packed_color.hpp>

#include <unordered_map>
#include <vector>

namespace voxen::land
{

// `Fake::[x|y|z]` bit sizes are hard-coded to this chunk size
static_assert(Consts::CHUNK_SIZE_BLOCKS == 32, "PseudoChunkData is hardcoded for 32-block chunks");

static_assert(sizeof(PseudoChunkData::Face) == 8, "8-byte packing of PseudoChunkData::Face is broken");

static_assert(sizeof(PseudoChunkData::EdgeEntry) == 10, "10-byte packing of PseudoChunkData::EdgeEntry is broken");
static_assert(sizeof(PseudoChunkData::CellEntry) == 16, "16-byte packing of PseudoChunkData::CellEntry is broken");

namespace
{

constexpr PackedColorSrgb FACE_COLOR_CODING[6] = {
	{ 255, 0, 0 },   // X+, red
	{ 255, 255, 0 }, // X-, yellow
	{ 0, 255, 0 },   // Y+, green
	{ 0, 255, 255 }, // Y-, turquoise
	{ 0, 0, 255 },   // Z+, blue
	{ 255, 0, 255 }, // Z-, purple
};

//                                   rrrrr gggggg bbbbb
constexpr uint16_t FAKE_COLOR_XP = 0b11111'000000'00000;
constexpr uint16_t FAKE_COLOR_XM = 0b11111'111111'00000;
constexpr uint16_t FAKE_COLOR_YP = 0b00000'111111'00000;
constexpr uint16_t FAKE_COLOR_YM = 0b00000'111111'11111;
constexpr uint16_t FAKE_COLOR_ZP = 0b00000'000000'11111;
constexpr uint16_t FAKE_COLOR_ZM = 0b11111'000000'11111;

} // namespace

PseudoChunkData::PseudoChunkData(ChunkAdjacencyRef ref)
{
	constexpr static uint32_t N = Consts::CHUNK_SIZE_BLOCKS;

	// Allocate on heap, expanded array is pretty large
	auto expanded_ids = std::make_unique<CubeArray<uint16_t, N + 2>>();
	ref.expandBlockIds(expanded_ids->view());

	std::vector<Face> faces;

	Utils::forYXZ<N>([&](uint32_t x, uint32_t y, uint32_t z) {
		uint16_t block_id = expanded_ids->load(x + 1, y + 1, z + 1);

		if (block_id == 0) {
			// Block has no visible faces
			return;
		}

		// TODO: check adjacency occlusion in a more generalized way.
		// Non-empty blocks might be not fully occluding in certain faces.

		// TODO: request "fake face color" from the block interface.
		// Currently using hardcoded face color-coding for debugging.

		if (expanded_ids->load(x + 2, y + 1, z + 1) == 0) {
			// X+ face visible
			faces.emplace_back(x, y, z, 0, 0, FACE_COLOR_CODING[0].toUint32());
		}

		if (expanded_ids->load(x, y + 1, z + 1) == 0) {
			// X- face visible
			faces.emplace_back(x, y, z, 1, 0, FACE_COLOR_CODING[1].toUint32());
		}

		if (expanded_ids->load(x + 1, y + 2, z + 1) == 0) {
			// Y+ face visible
			faces.emplace_back(x, y, z, 2, 0, FACE_COLOR_CODING[2].toUint32());
		}

		if (expanded_ids->load(x + 1, y, z + 1) == 0) {
			// Y- face visible
			faces.emplace_back(x, y, z, 3, 0, FACE_COLOR_CODING[3].toUint32());
		}

		if (expanded_ids->load(x + 1, y + 1, z + 2) == 0) {
			// Z+ face visible
			faces.emplace_back(x, y, z, 4, 0, FACE_COLOR_CODING[4].toUint32());
		}

		if (expanded_ids->load(x + 1, y + 1, z) == 0) {
			// Z- face visible
			faces.emplace_back(x, y, z, 5, 0, FACE_COLOR_CODING[5].toUint32());
		}
	});

	if (!faces.empty()) {
		m_faces = extras::dyn_array(faces.begin(), faces.end());
	}
}

PseudoChunkData::PseudoChunkData(std::span<const PseudoChunkData *, 8> hires)
{
	std::unordered_map<int32_t, glm::vec4> faces_combine[6];

	constexpr int32_t B = Consts::CHUNK_SIZE_BLOCKS;
	constexpr glm::ivec3 COORD_OFFSET[8] = {
		glm::ivec3(0, 0, B),
		glm::ivec3(B, 0, B),
		glm::ivec3(0, B, B),
		glm::ivec3(B, B, B),
		glm::ivec3(0, 0, 0),
		glm::ivec3(B, 0, 0),
		glm::ivec3(0, B, 0),
		glm::ivec3(B, B, 0),
	};

	for (size_t i = 0; i < 8; i++) {
		const PseudoChunkData *fcd = hires[i];
		if (!fcd) {
			continue;
		}

		for (const Face &face : fcd->m_faces) {
			glm::ivec3 coord(face.x, face.y, face.z);
			coord += COORD_OFFSET[i];
			coord /= 2;

			int32_t packed_coord = coord.x * 256 * 256 + coord.y * 256 + coord.z;
			faces_combine[face.orientation][packed_coord] += PackedColorSrgb(face.color_packed_srgb).toVec4();
		}
	}

	size_t num_faces = 0;
	for (uint32_t face = 0; face < 6; face++) {
		num_faces += faces_combine[face].size();
	}

	if (num_faces == 0) {
		return;
	}

	m_faces = extras::dyn_array<Face>(num_faces);

	size_t insert_pos = 0;
	for (uint32_t face = 0; face < 6; face++) {
		for (const auto &[key, value] : faces_combine[face]) {
			Face &ff = m_faces[insert_pos++];

			ff.x = uint32_t(key / (256 * 256));
			ff.y = uint32_t(key % (256 * 256) / 256);
			ff.z = uint32_t(key % 256);
			ff.orientation = face;
			ff.color_packed_srgb = PackedColorSrgb(value / value.w).toUint32();
		}
	}
}

namespace
{

struct HistogramAccumulatorEntry {
	uint16_t mat_id_or_color;
	bool is_color;
	float weight;
};

struct PointAccumulatorEntry {
	glm::vec3 cell_base_offset;
	glm::vec3 normal;
};

struct CellAccumulator {
	std::vector<HistogramAccumulatorEntry> histogram;
	std::vector<PointAccumulatorEntry> points;

	bool base_corner_sign = false;
	bool x_corner_sign = false;
	bool y_corner_sign = false;
	bool z_corner_sign = false;
};

bool isBlockEmpty(Chunk::BlockId block_id) noexcept
{
	return block_id == 0;
}

using EdgeEntry = PseudoChunkData::EdgeEntry;
using CellEntry = PseudoChunkData::CellEntry;

void resolveMaterialHistogram(std::span<HistogramAccumulatorEntry> entries, CellEntry &output)
{
	// We don't expect more than a few different entries
	// so perform aggregation with a dumb double loop
	for (size_t i = 1; i < entries.size(); /*nothing*/) {
		bool aggregated = false;
		// Try aggregating it with every entry before it
		for (size_t j = 0; j < i; j++) {
			// Must match both by value and is_color flag.
			// We're iterating from the beginning so all equal entries
			// are guaranteed to aggregate into one, the first of them.
			if (entries[i].mat_id_or_color == entries[j].mat_id_or_color && entries[i].is_color == entries[j].is_color) {
				entries[j].weight += entries[i].weight;
				aggregated = true;
				break;
			}
		}

		if (aggregated) {
			// Remove i-th entry
			std::swap(entries[i], entries.back());
			entries = entries.first(entries.size() - 1);
		} else {
			i++;
		}
	}

	// Sort by weight decreasing
	std::sort(entries.begin(), entries.end(),
		[](const HistogramAccumulatorEntry &a, const HistogramAccumulatorEntry &b) { return a.weight > b.weight; });

	const size_t num_entries = std::min<size_t>(entries.size(), 4);

	float weight_sum = 0.0f;
	for (size_t i = 0; i < num_entries; i++) {
		weight_sum += entries[i].weight;
	}

	for (size_t i = 0; i < num_entries; i++) {
		output.histogram_mat_id_or_color[i] = entries[i].mat_id_or_color;
		output.histogram_mat_weight[i] = static_cast<uint8_t>(255.0f * entries[i].weight / weight_sum);

		if (entries[i].is_color) {
			output.histogram_is_color_mask |= (1u << i);
		}
	}
}

void resolveCellAccumulators(extras::dyn_array<CellAccumulator> &cell_accumulators,
	extras::dyn_array<EdgeEntry> &edge_entries, extras::dyn_array<CellEntry> &cell_entries)
{
	size_t num_edge_entries = 0;
	size_t num_cell_entries = 0;

	// Count entries having some useful data
	for (const CellAccumulator &cell_accum : cell_accumulators) {
		num_edge_entries += cell_accum.base_corner_sign != cell_accum.x_corner_sign;
		num_edge_entries += cell_accum.base_corner_sign != cell_accum.y_corner_sign;
		num_edge_entries += cell_accum.base_corner_sign != cell_accum.y_corner_sign;
		num_cell_entries += !cell_accum.histogram.empty();
	}

	if (num_edge_entries == 0 && num_cell_entries == 0) {
		edge_entries = {};
		cell_entries = {};
		return;
	}

	edge_entries = extras::dyn_array<EdgeEntry>(num_edge_entries);
	cell_entries = extras::dyn_array<CellEntry>(num_cell_entries);

	size_t next_edge_entry = 0;
	size_t next_cell_entry = 0;

	constexpr uint32_t BLOCKS = Consts::CHUNK_SIZE_BLOCKS;
	assert(cell_accumulators.size() == BLOCKS * BLOCKS * BLOCKS);

	for (uint32_t cell = 0; cell < BLOCKS * BLOCKS * BLOCKS; cell++) {
		CellAccumulator &cell_accum = cell_accumulators[cell];

		const uint8_t y = static_cast<uint8_t>(cell / (BLOCKS * BLOCKS));
		const uint8_t x = static_cast<uint8_t>((cell / BLOCKS) % BLOCKS);
		const uint8_t z = static_cast<uint8_t>(cell % BLOCKS);

		// TODO: resolve edges
		(void) next_edge_entry;

		if (!cell_accum.histogram.empty()) {
			CellEntry &entry = cell_entries[next_cell_entry];
			entry = {};
			entry.x = x;
			entry.y = y;
			entry.z = z;

			resolveMaterialHistogram(cell_accum.histogram, entry);
			next_cell_entry++;
		}
	}

	// Sort entries to allow for fast lookups when building `PseudoChunkSurface`

	std::sort(edge_entries.begin(), edge_entries.end(), [](const EdgeEntry &a, const EdgeEntry &b) {
		return std::tie(a.y, a.x, a.z, a.axis) < std::tie(b.y, b.x, b.z, b.axis);
	});
	std::sort(cell_entries.begin(), cell_entries.end(),
		[](const CellEntry &a, const CellEntry &b) { return std::tie(a.y, a.x, a.z) < std::tie(b.y, b.x, b.z); });
}

} // namespace

void PseudoChunkData::generateFromLod0(std::span<const Chunk *const, 20> chunks)
{
	// Precomputed adjacent chunk indices.
	// For i-th "primary" chunk lists its X/Y/Z adjacent chunks' indices.
	constexpr size_t ADJACENT_INDEX[8][3] = {
		{ 2, 4, 1 },
		{ 3, 5, 8 },
		{ 12, 6, 3 },
		{ 13, 7, 9 },
		{ 6, 16, 5 },
		{ 7, 17, 10 },
		{ 14, 18, 7 },
		{ 15, 19, 11 },
	};

	constexpr uint32_t BLOCKS = Consts::CHUNK_SIZE_BLOCKS;

	// We will gather all cell information here
	extras::dyn_array<CellAccumulator> cell_accumulators(BLOCKS * BLOCKS * BLOCKS);

	// Allocate on heap, expanded array is pretty large
	auto expanded_ids = std::make_unique<CubeArray<Chunk::BlockId, BLOCKS>>();

	// Walk over "primary" chunks only
	for (size_t i = 0; i < 8; i++) {
		const Chunk *chunk = chunks[i];
		const Chunk *adj_x_chunk = chunks[ADJACENT_INDEX[i][0]];
		const Chunk *adj_y_chunk = chunks[ADJACENT_INDEX[i][1]];
		const Chunk *adj_z_chunk = chunks[ADJACENT_INDEX[i][2]];

		const uint32_t cell_add_x = (i & 0b010) ? BLOCKS : 0;
		const uint32_t cell_add_y = (i & 0b100) ? BLOCKS : 0;
		const uint32_t cell_add_z = (i & 0b001) ? BLOCKS : 0;

		// Expand block IDs for this chunk - we will iterate over them a lot.
		// For adjacent chunks it is probably not useful - we will visit
		// just a few border blocks there.
		chunk->blockIds().expand(expanded_ids->view());

		Utils::forYXZ<BLOCKS>([&](uint32_t x, uint32_t y, uint32_t z) {
			const Chunk::BlockId this_block_id = expanded_ids->load(x, y, z);
			Chunk::BlockId adj_x_block_id, adj_y_block_id, adj_z_block_id;

			if (x + 1 < BLOCKS) {
				adj_x_block_id = expanded_ids->load(x + 1, y, z);
			} else {
				adj_x_block_id = adj_x_chunk->blockIds().load(0, y, z);
			}

			if (y + 1 < BLOCKS) {
				adj_y_block_id = expanded_ids->load(x, y + 1, z);
			} else {
				adj_y_block_id = adj_y_chunk->blockIds().load(x, 0, z);
			}

			if (z + 1 < BLOCKS) {
				adj_z_block_id = expanded_ids->load(x, y, z + 1);
			} else {
				adj_z_block_id = adj_z_chunk->blockIds().load(x, y, 0);
			}

			bool this_block_empty = isBlockEmpty(this_block_id);
			bool adj_x_block_empty = isBlockEmpty(adj_x_block_id);
			bool adj_y_block_empty = isBlockEmpty(adj_y_block_id);
			bool adj_z_block_empty = isBlockEmpty(adj_z_block_id);

			uint32_t cell_index = (y + cell_add_y) / 2 * BLOCKS * BLOCKS;
			cell_index += (x + cell_add_x) / 2 * BLOCKS;
			cell_index += (z + cell_add_z) / 2;
			CellAccumulator &cell_accum = cell_accumulators[cell_index];

			if (!((x | y | z) & 1)) {
				// All coordinates even - this block determines the sign of base cell corner
				cell_accum.base_corner_sign = !this_block_empty;
			} else if (!((x | y) & 1)) {
				// XY even, Z odd - end of Z edge determines the sign of cell corner
				cell_accum.z_corner_sign = !adj_z_block_empty;
			} else if (!((x | z) & 1)) {
				// XZ even, Y odd - end of Y edge determines the sign of cell corner
				cell_accum.y_corner_sign = !adj_y_block_empty;
			} else if (!((y | z) & 1)) {
				// YZ even, X odd - end of X edge determines the sign of cell corner
				cell_accum.x_corner_sign = !adj_x_block_empty;
			}

			const glm::vec3 cell_base_offset = glm::vec3(x & 1, y & 1, z & 1) * 0.5f;

			if (this_block_empty != adj_x_block_empty) {
				// We have an X edge
				glm::vec3 point = cell_base_offset;
				point.x += 0.5f;

				glm::vec3 normal(this_block_empty ? -1.0f : 1.0f, 0.0f, 0.0f);

				cell_accum.points.emplace_back(point, normal);
				// TODO: select color/material from block
				cell_accum.histogram.emplace_back(this_block_empty ? FAKE_COLOR_XM : FAKE_COLOR_XP, true, 1.0f);
			}

			if (this_block_empty != adj_y_block_empty) {
				// We have an Y edge
				glm::vec3 point = cell_base_offset;
				point.x += 0.5f;

				glm::vec3 normal(0.0f, this_block_empty ? -1.0f : 1.0f, 0.0f);

				cell_accum.points.emplace_back(point, normal);
				// TODO: select color/material from block
				cell_accum.histogram.emplace_back(this_block_empty ? FAKE_COLOR_YM : FAKE_COLOR_YP, true, 1.0f);
			}

			if (this_block_empty != adj_z_block_empty) {
				// We have a Z edge
				glm::vec3 point = cell_base_offset;
				point.z += 0.5f;

				glm::vec3 normal(0.0f, 0.0f, this_block_empty ? -1.0f : 1.0f);

				cell_accum.points.emplace_back(point, normal);
				// TODO: select color/material from block
				cell_accum.histogram.emplace_back(this_block_empty ? FAKE_COLOR_ZM : FAKE_COLOR_ZP, true, 1.0f);
			}
		});
	}

	resolveCellAccumulators(cell_accumulators, m_edge_entries, m_cell_entries);
}

void PseudoChunkData::generateFromFinerLod(std::span<const PseudoChunkData *const, 8> finer)
{
	constexpr uint32_t BLOCKS = Consts::CHUNK_SIZE_BLOCKS;

	// We will gather all cell information here
	extras::dyn_array<CellAccumulator> cell_accumulators(BLOCKS * BLOCKS * BLOCKS);

	for (size_t i = 0; i < 8; i++) {
		const PseudoChunkData *finer_data = finer[i];

		const uint32_t cell_add_x = (i & 0b010) ? BLOCKS : 0;
		const uint32_t cell_add_y = (i & 0b100) ? BLOCKS : 0;
		const uint32_t cell_add_z = (i & 0b001) ? BLOCKS : 0;

		for (const EdgeEntry &edge : finer_data->edgeEntries()) {
			uint32_t x = edge.x + cell_add_x;
			uint32_t y = edge.y + cell_add_y;
			uint32_t z = edge.z + cell_add_z;

			uint32_t cell_index = (y / 2 * BLOCKS + x / 2) * BLOCKS + z / 2;
			CellAccumulator &cell_accum = cell_accumulators[cell_index];

			if (!((x | y | z) & 1)) {
				// All coordinates even - lower endpoint of this edge is the base cell corner
				cell_accum.base_corner_sign = edge.lower_endpoint_solid;
			} else if (!((x | y) & 1)) {
				// XY even, Z odd - higher endpoint of this edge is Z cell corner
				cell_accum.z_corner_sign = !edge.lower_endpoint_solid;
			} else if (!((x | z) & 1)) {
				// XZ even, Y odd - higher endpoint of this edge is Y cell corner
				cell_accum.y_corner_sign = !edge.lower_endpoint_solid;
			} else if (!((y | z) & 1)) {
				// YZ even, X odd - higher endpoint of this edge is X cell corner
				cell_accum.x_corner_sign = !edge.lower_endpoint_solid;
			}

			glm::vec3 cell_base_offset = glm::vec3(x & 1, y & 1, z & 1) * 0.5f;
			cell_base_offset[edge.axis] += float(edge.offset_unorm) / (2.0f * 65535.0f);

			glm::vec3 normal;
			normal.x = float(edge.normal_x_snorm) / 32767.0f;
			normal.z = float(edge.normal_z_snorm) / 32767.0f;
			normal.y = (edge.normal_y_sign ? -1.0f : 1.0f) * std::sqrt(1.0f - normal.x * normal.x - normal.z * normal.z);

			cell_accum.points.emplace_back(cell_base_offset, normal);
		}

		for (const CellEntry &cell : finer_data->cellEntries()) {
			uint32_t x = cell.x + cell_add_x;
			uint32_t y = cell.y + cell_add_y;
			uint32_t z = cell.z + cell_add_z;

			uint32_t cell_index = (y / 2 * BLOCKS + x / 2) * BLOCKS + z / 2;
			CellAccumulator &cell_accum = cell_accumulators[cell_index];

			for (int j = 0; j < 4; j++) {
				if (cell.histogram_mat_weight[j] == 0) {
					continue;
				}

				cell_accum.histogram.emplace_back(HistogramAccumulatorEntry {
					.mat_id_or_color = cell.histogram_mat_id_or_color[i],
					.is_color = !!(cell.histogram_is_color_mask & (1 << j)),
					.weight = float(cell.histogram_mat_weight[j]) / 255.0f,
				});
			}
		}
	}

	resolveCellAccumulators(cell_accumulators, m_edge_entries, m_cell_entries);
}

} // namespace voxen::land
