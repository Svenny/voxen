#include <voxen/land/pseudo_chunk_surface.hpp>

#include <voxen/land/cube_array.hpp>
#include <voxen/land/land_chunk.hpp>
#include <voxen/land/land_public_consts.hpp>
#include <voxen/land/land_temp_blocks.hpp>
#include <voxen/land/land_utils.hpp>
#include <voxen/land/pseudo_chunk_data.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/packed_color.hpp>

#include "land_geometry_utils_private.hpp"

#include <glm/gtc/packing.hpp>

#include <cassert>
#include <unordered_map>

namespace voxen::land
{

static_assert(sizeof(PseudoSurfaceVertexPosition) == 6, "6-byte packing of PseudoSurfaceVertexPosition is broken");
static_assert(sizeof(PseudoSurfaceVertexAttributes) == 16,
	"16-byte packing of PseudoSurfaceVertexAttributes is broken");

using detail::SurfaceMatHistEntry;
using CellEntry = PseudoChunkData::CellEntry;

namespace
{

struct VertexState {
	glm::vec3 position;
	glm::vec4 color_accumulator;
	glm::vec3 normal_accumulator;
	uint32_t vertex_index;
};

struct ProcessedCellEntry {
	glm::vec3 position_mainchunk_local_space;
	SurfaceMatHistEntry mat_hist[4] = {};
};

constexpr glm::ivec3 FACE_COORD_OFFSET[6][4] = {
	{ glm::ivec3(1, 0, 1), glm::ivec3(1, 0, 0), glm::ivec3(1, 1, 0), glm::ivec3(1, 1, 1) }, // 3 2 6 7
	{ glm::ivec3(0, 1, 0), glm::ivec3(0, 0, 0), glm::ivec3(0, 0, 1), glm::ivec3(0, 1, 1) }, // 4 0 1 5
	{ glm::ivec3(1, 1, 0), glm::ivec3(0, 1, 0), glm::ivec3(0, 1, 1), glm::ivec3(1, 1, 1) }, // 6 4 5 7
	{ glm::ivec3(0, 0, 1), glm::ivec3(0, 0, 0), glm::ivec3(1, 0, 0), glm::ivec3(1, 0, 1) }, // 1 0 2 3
	{ glm::ivec3(0, 1, 1), glm::ivec3(0, 0, 1), glm::ivec3(1, 0, 1), glm::ivec3(1, 1, 1) }, // 5 1 3 7
	{ glm::ivec3(1, 0, 0), glm::ivec3(0, 0, 0), glm::ivec3(0, 1, 0), glm::ivec3(1, 1, 0) }, // 2 0 4 6
};

constexpr glm::vec3 FACE_NORMAL[6] = {
	glm::vec3(1, 0, 0),
	glm::vec3(-1, 0, 0),
	glm::vec3(0, 1, 0),
	glm::vec3(0, -1, 0),
	glm::vec3(0, 0, 1),
	glm::vec3(0, 0, -1),
};

// Store vertex position, `pos` must be in [-0.125:1.125] range or it will get clipped
void packVertexPosition(glm::vec3 pos, PseudoSurfaceVertexPosition &output) noexcept
{
	output.position_unorm = glm::packUnorm<uint16_t>(pos * 0.8f + 0.1f);
}

// Store vertex normal, `normal` must have unit length
void packNormal(glm::vec3 normal, PseudoSurfaceVertexAttributes &output) noexcept
{
	// Project the sphere onto the octahedron, and then onto the XZ plane
	float inv = 1.0f / (std::abs(normal.x) + std::abs(normal.y) + std::abs(normal.z));
	glm::vec2 p = glm::vec2(normal.x, normal.z) * inv;
	glm::vec2 result;

	// Reflect the folds of the lower hemisphere over the diagonals.
	// This code has swapped Y and Z axes relative to the original paper.
	// That's because in our engine Y is oriented "up" and we expect normals with
	// negative Y to be the least frequent, reducing the need to take this path.
	if (normal.y <= 0.0f) {
		result.x = (1.0f - std::abs(p.y)) * (p.x >= 0.0f ? 1.0f : -1.0f);
		result.y = (1.0f - std::abs(p.x)) * (p.y >= 0.0f ? 1.0f : -1.0f);
	} else {
		result = p;
	}

	output.normal_oct_snorm = glm::packSnorm<int16_t>(result);
}

glm::vec3 adjustSkirtPosition(glm::vec3 pos, const glm::vec3 &normal, uint32_t lod) noexcept
{
	if (pos.x < 0.0f || pos.y < 0.0f || pos.z < 0.0f) {
		pos -= (lod == 1 ? 0.03f : 0.01f) * normal;
	}

	return pos;
}

void packMaterialHistogram(const ProcessedCellEntry &c0, const ProcessedCellEntry &c1, const ProcessedCellEntry &c2,
	PseudoSurfaceVertexAttributes &a0, PseudoSurfaceVertexAttributes &a1, PseudoSurfaceVertexAttributes &a2) noexcept
{
	std::vector<SurfaceMatHistEntry> entries;

	for (int i = 0; i < 4; i++) {
		detail::GeometryUtils::addMatHistEntry(entries, c0.mat_hist[i]);
		detail::GeometryUtils::addMatHistEntry(entries, c1.mat_hist[i]);
		detail::GeometryUtils::addMatHistEntry(entries, c2.mat_hist[i]);
	}

	// Sort by weight decreasing
	std::sort(entries.begin(), entries.end(),
		[](const SurfaceMatHistEntry &a, const SurfaceMatHistEntry &b) { return a.weight > b.weight; });

	// Push zero entries to pad up to four (simplifies logic ahead)
	while (entries.size() < 4) {
		entries.emplace_back();
	}

	auto resolve_vertex = [&](const ProcessedCellEntry &in, PseudoSurfaceVertexAttributes &out) {
		glm::vec4 weights(0.0f);

		for (size_t i = 0; i < 4; i++) {
			int ii = static_cast<int>(i);
			out.mat_hist_entries[ii] = entries[i].mat_id_or_color;

			for (size_t j = 0; j < 4; j++) {
				if (in.mat_hist[j].mat_id_or_color == entries[i].mat_id_or_color) {
					weights[ii] += in.mat_hist[j].weight;
				}
			}
		}

		float weight_sum = std::max(0.000001f, weights.x + weights.y + weights.z + weights.w);
		out.mat_hist_weights = glm::packUnorm<uint8_t>(weights / weight_sum);
	};

	resolve_vertex(c0, a0);
	resolve_vertex(c1, a1);
	resolve_vertex(c2, a2);
}

} // namespace

void PseudoChunkSurface::generate(ChunkAdjacencyRef adj)
{
	m_vertex_positions.clear();
	m_vertex_attributes.clear();
	m_indices.clear();

	constexpr static uint32_t B = Consts::CHUNK_SIZE_BLOCKS;

	// Allocate on heap, expanded array is pretty large
	auto expanded_ids = std::make_unique<CubeArray<uint16_t, B + 2>>();
	adj.expandBlockIds(expanded_ids->view());

	std::unordered_map<int32_t, VertexState> vertex_state;
	std::vector<uint32_t> index_buffer;

	auto get_vertex_state = [&](int32_t key) -> VertexState & {
		auto iter = vertex_state.find(key);
		if (iter != vertex_state.end()) {
			return iter->second;
		}

		int32_t x = (key / 6) >> 14;
		int32_t y = ((key / 6) >> 7) % (1 << 7);
		int32_t z = (key / 6) % (1 << 7);

		auto &state = vertex_state[key];
		state.position = glm::vec3(x, y, z);
		state.vertex_index = static_cast<uint32_t>(m_vertex_positions.size());
		m_vertex_positions.emplace_back();
		m_vertex_attributes.emplace_back();
		return state;
	};

	auto add_face = [&](uint32_t x, uint32_t y, uint32_t z, int32_t orientation, PackedColorSrgb color) {
		glm::ivec3 base_coord(x, y, z);

		uint32_t vertex_indices[4];

		for (int i = 0; i < 4; i++) {
			glm::ivec3 vertex_coord = base_coord + FACE_COORD_OFFSET[orientation][i];
			int32_t vertex_key = ((vertex_coord.x << 14) + (vertex_coord.y << 7) + vertex_coord.z) * 6 + orientation;
			VertexState &state = get_vertex_state(vertex_key);

			state.color_accumulator += color.toVec4();
			state.normal_accumulator += FACE_NORMAL[orientation];
			vertex_indices[i] = state.vertex_index;
		}

		// First triangle
		index_buffer.emplace_back(vertex_indices[0]);
		index_buffer.emplace_back(vertex_indices[1]);
		index_buffer.emplace_back(vertex_indices[2]);
		// Second triangle
		index_buffer.emplace_back(vertex_indices[2]);
		index_buffer.emplace_back(vertex_indices[3]);
		index_buffer.emplace_back(vertex_indices[0]);
	};

	Utils::forYXZ<B>([&](uint32_t x, uint32_t y, uint32_t z) {
		// TODO: check adjacency occlusion in a more generalized way.
		// Non-empty blocks might be not fully occluding in certain faces.

		// TODO: request "fake face color" from the block interface.
		// Currently using hardcoded face color-coding for debugging.

		Chunk::BlockId block_id = expanded_ids->load(x + 1, y + 1, z + 1);

		if (TempBlockMeta::isBlockEmpty(block_id)) {
			// Block has no visible faces
			return;
		}

		PackedColorSrgb block_color = TempBlockMeta::BLOCK_FIXED_COLOR[block_id];

		if (TempBlockMeta::isBlockEmpty(expanded_ids->load(x + 2, y + 1, z + 1))) {
			// X+ face visible
			add_face(x, y, z, 0, block_color);
		}

		if (TempBlockMeta::isBlockEmpty(expanded_ids->load(x, y + 1, z + 1))) {
			// X- face visible
			add_face(x, y, z, 1, block_color);
		}

		if (TempBlockMeta::isBlockEmpty(expanded_ids->load(x + 1, y + 2, z + 1))) {
			// Y+ face visible
			add_face(x, y, z, 2, block_color);
		}

		if (TempBlockMeta::isBlockEmpty(expanded_ids->load(x + 1, y, z + 1))) {
			// Y- face visible
			add_face(x, y, z, 3, block_color);
		}

		if (TempBlockMeta::isBlockEmpty(expanded_ids->load(x + 1, y + 1, z + 2))) {
			// Z+ face visible
			add_face(x, y, z, 4, block_color);
		}

		if (TempBlockMeta::isBlockEmpty(expanded_ids->load(x + 1, y + 1, z))) {
			// Z- face visible
			add_face(x, y, z, 5, block_color);
		}
	});

	constexpr float DIVISOR = 1.0f / static_cast<float>(B);

	for (const auto &[key, state] : vertex_state) {
		auto &vertex_pos = m_vertex_positions[state.vertex_index];
		auto &vertex_attrib = m_vertex_attributes[state.vertex_index];

		glm::vec3 pos_normalized = state.position * DIVISOR;
		packVertexPosition(pos_normalized, vertex_pos);

		glm::vec3 normal = glm::normalize(state.normal_accumulator);
		packNormal(normal, vertex_attrib);

		PackedColorSrgb packed_color(glm::vec3(state.color_accumulator) / state.color_accumulator.a);
		uint16_t color = TempBlockMeta::packColor555(packed_color);

		vertex_attrib.mat_hist_entries = glm::u16vec4(color, 0, 0, 0);
		vertex_attrib.mat_hist_weights = glm::u8vec4(255, 0, 0, 0);
	}

	m_indices.resize(index_buffer.size());
	for (size_t i = 0; i < index_buffer.size(); i++) {
		assert(index_buffer[i] <= UINT16_MAX);
		m_indices[i] = static_cast<uint16_t>(index_buffer[i]);
	}
}

void PseudoChunkSurface::generate(std::span<const PseudoChunkData *const, 19> datas, uint32_t lod)
{
	m_vertex_positions.clear();
	m_vertex_attributes.clear();
	m_indices.clear();

	constexpr uint32_t B = Consts::CHUNK_SIZE_BLOCKS;

	// `v0`, `v1`, `v2` must be in the correct winding order (front-face is clockwise)
	auto add_triangle = [&](const ProcessedCellEntry &v0, const ProcessedCellEntry &v1, const ProcessedCellEntry &v2) {
		glm::vec3 v10 = v0.position_mainchunk_local_space - v1.position_mainchunk_local_space;
		glm::vec3 v12 = v2.position_mainchunk_local_space - v1.position_mainchunk_local_space;
		// TODO: something is wrong, negation is not needed
		glm::vec3 normal = glm::normalize(-glm::cross(v10, v12));

		// TODO: use indexing!
		// No need to repeat the same vertices if there is no significant difference
		// in normals (not a sharp feature) and material histograms can be matched.
		uint32_t first_new_index = static_cast<uint32_t>(m_indices.size());
		m_indices.emplace_back(first_new_index);
		m_indices.emplace_back(first_new_index + 1);
		m_indices.emplace_back(first_new_index + 2);

		m_vertex_positions.emplace_back();
		m_vertex_positions.emplace_back();
		m_vertex_positions.emplace_back();

		m_vertex_attributes.emplace_back();
		m_vertex_attributes.emplace_back();
		m_vertex_attributes.emplace_back();

		PseudoSurfaceVertexPosition &pp0 = m_vertex_positions[first_new_index];
		PseudoSurfaceVertexPosition &pp1 = m_vertex_positions[first_new_index + 1];
		PseudoSurfaceVertexPosition &pp2 = m_vertex_positions[first_new_index + 2];

		PseudoSurfaceVertexAttributes &pa0 = m_vertex_attributes[first_new_index];
		PseudoSurfaceVertexAttributes &pa1 = m_vertex_attributes[first_new_index + 1];
		PseudoSurfaceVertexAttributes &pa2 = m_vertex_attributes[first_new_index + 2];

		packVertexPosition(adjustSkirtPosition(v0.position_mainchunk_local_space, normal, lod), pp0);
		packVertexPosition(adjustSkirtPosition(v1.position_mainchunk_local_space, normal, lod), pp1);
		packVertexPosition(adjustSkirtPosition(v2.position_mainchunk_local_space, normal, lod), pp2);

		packNormal(normal, pa0);
		pa1.normal_oct_snorm = pa0.normal_oct_snorm;
		pa2.normal_oct_snorm = pa0.normal_oct_snorm;

		packMaterialHistogram(v0, v1, v2, pa0, pa1, pa2);
	};

	auto add_quad = [&](const ProcessedCellEntry cells[4], bool lower_solid) {
		// Order is:
		// 01
		// 23
		// Try triangles 0-1-2, 1-3-2
		if (lower_solid) {
			add_triangle(cells[0], cells[1], cells[2]);
			add_triangle(cells[1], cells[3], cells[2]);
		} else {
			add_triangle(cells[0], cells[2], cells[1]);
			add_triangle(cells[1], cells[2], cells[3]);
		}
	};

	constexpr glm::ivec3 EDGE_AXIS_OFFSETS[3][3] = {
		// X edges
		{ glm::ivec3(0, -1, 0), glm::ivec3(0, 0, -1), glm::ivec3(0, -1, -1) },
		// Y edges
		{ glm::ivec3(0, 0, -1), glm::ivec3(-1, 0, 0), glm::ivec3(-1, 0, -1) },
		// Z edges
		{ glm::ivec3(-1, 0, 0), glm::ivec3(0, -1, 0), glm::ivec3(-1, -1, 0) },
	};

	constexpr uint32_t DATA_INDEX_3CUBE_MAP[27] = { // Y < 0
		UINT32_MAX, 15, UINT32_MAX, 7, 4, 8, UINT32_MAX, 16, UINT32_MAX,
		// Y >= 0 && Y < B
		11, 2, 12, 6, 0, 5, 13, 1, 14,
		// Y >= B
		UINT32_MAX, 17, UINT32_MAX, 9, 3, 10, UINT32_MAX, 18, UINT32_MAX
	};

	auto add_edge = [&](glm::ivec3 edge_base_coord, int axis, bool lower_solid) {
		ProcessedCellEntry processed_cells[4];

		// Load four cells adjacent to this edge
		for (int i = 0; i < 4; i++) {
			glm::ivec3 coord = edge_base_coord;
			if (i < 3) {
				coord += EDGE_AXIS_OFFSETS[axis][2 - i];
			}

			uint32_t data_index_3cube = 0;
			glm::vec3 adjustment(0.0f);

			if (coord.y < 0) {
				coord.y += B;
				adjustment.y = -1.0f;
			} else if (coord.y >= int32_t(B)) {
				coord.y -= B;
				data_index_3cube += 18;
				adjustment.y = +1.0f;
			} else {
				data_index_3cube += 9;
			}

			if (coord.x < 0) {
				coord.x += B;
				adjustment.x = -1.0f;
			} else if (coord.x >= int32_t(B)) {
				coord.x -= B;
				data_index_3cube += 6;
				adjustment.x = +1.0f;
			} else {
				data_index_3cube += 3;
			}

			if (coord.z < 0) {
				coord.z += B;
				adjustment.z = -1.0f;
			} else if (coord.z >= int32_t(B)) {
				coord.z -= B;
				data_index_3cube += 2;
				adjustment.z = +1.0f;
			} else {
				data_index_3cube += 1;
			}

			const uint32_t data_index = DATA_INDEX_3CUBE_MAP[data_index_3cube];
			// We must never index into vertex-adjacent chunks
			assert(data_index != UINT32_MAX);

			const PseudoChunkData::CellEntry *cell = datas[data_index]->findEntry(coord);

			if (cell) [[likely]] {
				// Have cell - this is expected to be true nearly always.
				processed_cells[i].position_mainchunk_local_space = glm::unpackUnorm<float>(cell->surface_point_unorm)
					+ adjustment;
				detail::GeometryUtils::unpackCellEntryMatHist(processed_cells[i].mat_hist, *cell);
			} else {
				// Don't have the cell - this actually means topology inconsistency between pseudo-chunks.
				// It can happen as a rare edge (pun intended) case when directly-generated
				// and aggregated pseudo-chunks meet at the boundary. This will "cure" itself naturally
				// as more true chunks are generated, but for while simply substitute cell midpoint.
				glm::vec3 bogus_position = (glm::vec3(coord) + 0.5f) / float(Consts::CHUNK_SIZE_BLOCKS) + adjustment;
				processed_cells[i].position_mainchunk_local_space = bogus_position;
				processed_cells[i].mat_hist[0].mat_id_or_color = 0b1'11111'00000'00000;
				processed_cells[i].mat_hist[0].weight = 255;
			}
		}

		add_quad(processed_cells, lower_solid);
	};

	const auto &main_cells = datas[0]->cellEntries();

	// Specialization of `add_edge` for inner edges (all adjacent cells are from `main_cells`)
	auto add_inner_edge = [&](glm::ivec3 edge_base_coord, int axis, bool lower_solid, const CellEntry &cell) {
		ProcessedCellEntry processed_cells[4];

		const CellEntry *cells[4];
		cells[0] = datas[0]->findEntry(edge_base_coord + EDGE_AXIS_OFFSETS[axis][2]);
		cells[1] = datas[0]->findEntry(edge_base_coord + EDGE_AXIS_OFFSETS[axis][1]);
		cells[2] = datas[0]->findEntry(edge_base_coord + EDGE_AXIS_OFFSETS[axis][0]);
		cells[3] = &cell;

		for (int i = 0; i < 4; i++) {
			// TODO: all cells must be present, this covers a bug in our generator
			if (cells[i]) {
				processed_cells[i].position_mainchunk_local_space = glm::unpackUnorm<float>(cells[i]->surface_point_unorm);
				detail::GeometryUtils::unpackCellEntryMatHist(processed_cells[i].mat_hist, *cells[i]);
			} else {
				processed_cells[i].position_mainchunk_local_space
					= (glm::vec3(edge_base_coord + EDGE_AXIS_OFFSETS[axis][2 - i]) + 0.5f)
					/ float(Consts::CHUNK_SIZE_BLOCKS);
			}
		}

		add_quad(processed_cells, lower_solid);
	};

	// Visit cells of the chunk we're generating surface for.
	// Add geometry (pair of quads) for every surface-crossing edge met.
	//
	// XXX: there are entirely too many bit operations and conditionals.
	// Could we factor more of this out into precomputed tables?
	for (size_t i = 0; i < main_cells.size(); i++) {
		const CellEntry &cell = main_cells[i];

		constexpr uint8_t EDGE_MASKS[12] = {
			// X edges
			0b00000101, // [0], 0-2, base edge
			0b00001010, // [1], 1-3, Z border edge
			0b01010000, // [2], 4-6, Y border edge
			0b10100000, // [3], 5-7, Y/Z border edge
			// Y edges
			0b00010001, // [4], 0-4, base edge
			0b00100010, // [5], 1-5, Z border edge
			0b01000100, // [6], 2-6, X border edge
			0b10001000, // [7], 3-7, X/Z border edge
			// Z edges
			0b00000011, // [8], 0-1, base edge
			0b00001100, // [9], 2-3, X border edge
			0b00110000, // [10], 4-5, Y border edge
			0b11000000, // [11], 6-7, X/Y border edge
		};

		bool solid[7];
		for (int bit = 0; bit < 7; bit++) {
			solid[bit] = !!(cell.corner_solid_mask & (1 << bit));
		}

		bool has_edge[12];
		for (int edge = 0; edge < 12; edge++) {
			uint8_t test_mask = cell.corner_solid_mask & EDGE_MASKS[edge];
			has_edge[edge] = test_mask != 0 && test_mask != EDGE_MASKS[edge];
		}

		glm::ivec3 cell_index(cell.cell_index);

		if (has_edge[0]) {
			cell_index.y > 0 && cell_index.z > 0
				? add_inner_edge(cell_index, 0, solid[0], cell)
				: add_edge(cell_index, 0, solid[0]);
		}
		if (has_edge[4]) {
			cell_index.x > 0 && cell_index.z > 0
				? add_inner_edge(cell_index, 1, solid[0], cell)
				: add_edge(cell_index, 1, solid[0]);
		}
		if (has_edge[8]) {
			cell_index.x > 0 && cell_index.y > 0
				? add_inner_edge(cell_index, 2, solid[0], cell)
				: add_edge(cell_index, 2, solid[0]);
		}

		const bool x_border = cell_index.x + 1 == B;
		const bool y_border = cell_index.y + 1 == B;
		const bool z_border = cell_index.z + 1 == B;

		if (x_border) {
			if (has_edge[6]) {
				add_edge(cell_index + glm::ivec3(1, 0, 0), 1, solid[2]);
			}
			if (has_edge[9]) {
				add_edge(cell_index + glm::ivec3(1, 0, 0), 2, solid[2]);
			}
		}
		if (y_border) {
			if (has_edge[2]) {
				add_edge(cell_index + glm::ivec3(0, 1, 0), 0, solid[4]);
			}
			if (has_edge[10]) {
				add_edge(cell_index + glm::ivec3(0, 1, 0), 2, solid[4]);
			}
		}
		if (z_border) {
			if (has_edge[1]) {
				add_edge(cell_index + glm::ivec3(0, 0, 1), 0, solid[1]);
			}
			if (has_edge[5]) {
				add_edge(cell_index + glm::ivec3(0, 0, 1), 1, solid[1]);
			}
		}

		if (has_edge[3] && (y_border || z_border)) {
			add_edge(cell_index + glm::ivec3(0, 1, 1), 0, solid[5]);
		}
		if (has_edge[7] && (x_border || z_border)) {
			add_edge(cell_index + glm::ivec3(1, 0, 1), 1, solid[3]);
		}
		if (has_edge[11] && (x_border || y_border)) {
			add_edge(cell_index + glm::ivec3(1, 1, 0), 2, solid[6]);
		}
	}
}

} // namespace voxen::land
