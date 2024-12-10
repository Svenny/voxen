#include <voxen/land/pseudo_chunk_surface.hpp>

#include <voxen/land/land_public_consts.hpp>
#include <voxen/land/pseudo_chunk_data.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/packed_color.hpp>

#include <glm/glm.hpp>

#include <cassert>
#include <unordered_map>

namespace voxen::land
{

static_assert(sizeof(PseudoSurfaceVertexPosition) == 6, "6-byte packing of PseudoSurfaceVertexPosition is broken");
static_assert(sizeof(PseudoSurfaceVertexAttributes) == 18,
	"18-byte packing of PseudoSurfaceVertexAttributes is broken");

namespace
{

struct VertexState {
	glm::vec3 position;
	glm::vec4 color_accumulator;
	glm::vec3 normal_accumulator;
	uint32_t vertex_index;
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

} // namespace

void PseudoChunkSurface::generate(const PseudoChunkData &data)
{
	m_vertex_positions = {};
	m_vertex_attributes = {};
	m_indices = {};

	if (data.empty()) {
		return;
	}

	std::unordered_map<int32_t, VertexState> vertex_state;
	std::vector<PseudoSurfaceVertexPosition> vertex_position_buffer;
	std::vector<PseudoSurfaceVertexAttributes> vertex_attrib_buffer;
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
		state.vertex_index = static_cast<uint32_t>(vertex_position_buffer.size());
		vertex_position_buffer.emplace_back();
		vertex_attrib_buffer.emplace_back();
		return state;
	};

	for (const PseudoChunkData::Face &face : data.faces()) {
		glm::ivec3 base_coord(face.x, face.y, face.z);

		uint32_t vertex_indices[4];

		for (int i = 0; i < 4; i++) {
			glm::ivec3 vertex_coord = base_coord + FACE_COORD_OFFSET[face.orientation][i];
			int32_t vertex_key = ((vertex_coord.x << 14) + (vertex_coord.y << 7) + vertex_coord.z) * 6 + face.orientation;
			VertexState &state = get_vertex_state(vertex_key);

			state.color_accumulator += PackedColorSrgb(face.color_packed_srgb).toVec4();
			state.normal_accumulator += FACE_NORMAL[face.orientation];
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
	}

	constexpr float DIVISOR = 1.0f / static_cast<float>(Consts::CHUNK_SIZE_BLOCKS);

	for (const auto &[key, state] : vertex_state) {
		auto &vertex_pos = vertex_position_buffer[state.vertex_index];
		auto &vertex_attrib = vertex_attrib_buffer[state.vertex_index];

		glm::vec3 pos_normalized = state.position * DIVISOR;
		vertex_pos.position_x_unorm = static_cast<uint16_t>(pos_normalized.x * UINT16_MAX);
		vertex_pos.position_y_unorm = static_cast<uint16_t>(pos_normalized.y * UINT16_MAX);
		vertex_pos.position_z_unorm = static_cast<uint16_t>(pos_normalized.z * UINT16_MAX);

		glm::vec3 normal = glm::normalize(state.normal_accumulator);
		vertex_attrib.normal_x_snorm = static_cast<int16_t>(normal.x * INT16_MAX);
		vertex_attrib.normal_y_snorm = static_cast<int16_t>(normal.y * INT16_MAX);
		vertex_attrib.normal_z_snorm = static_cast<int16_t>(normal.z * INT16_MAX);

		PackedColorSrgb packed_color(glm::vec3(state.color_accumulator) / state.color_accumulator.a);
		uint16_t color16 = uint16_t((packed_color.r >> 3) << 11);
		color16 |= uint16_t((packed_color.g >> 2) << 5);
		color16 |= packed_color.b >> 3;

		vertex_attrib.histogram_mat_id_or_color[0] = color16;
		vertex_attrib.histogram_mat_weight[0] = 255;

		for (int i = 1; i < 4; i++) {
			vertex_attrib.histogram_mat_id_or_color[i] = 0;
			vertex_attrib.histogram_mat_weight[i] = 0;
		}
	}

	m_vertex_positions = extras::dyn_array<PseudoSurfaceVertexPosition>(vertex_position_buffer.begin(),
		vertex_position_buffer.end());
	m_vertex_attributes = extras::dyn_array<PseudoSurfaceVertexAttributes>(vertex_attrib_buffer.begin(),
		vertex_attrib_buffer.end());

	m_indices = extras::dyn_array<uint16_t>(index_buffer.size(), [&index_buffer](void *place, size_t index) {
		assert(index_buffer[index] <= UINT16_MAX);
		new (place) uint16_t(static_cast<uint16_t>(index_buffer[index]));
	});
}

namespace
{

struct CellAccumulator {
	QefSolver qef_solver;
	PseudoChunkData::CellEntry materials;
	size_t vertex_index = SIZE_MAX;
};

int32_t getCellIndex(glm::ivec3 cell_coord) noexcept
{
	constexpr int32_t MULT = 3 * Consts::CHUNK_SIZE_BLOCKS;
	cell_coord += Consts::CHUNK_SIZE_BLOCKS;
	return cell_coord.y * MULT * MULT + cell_coord.x * MULT + cell_coord.z;
}

} // namespace

void PseudoChunkSurface::generate(std::span<const PseudoChunkData *const, 21> datas)
{
	m_vertex_positions = {};
	m_vertex_attributes = {};
	m_indices = {};

	std::unordered_map<int32_t, CellAccumulator> cell_accumulator_map;

	constexpr int32_t MAX_COORD = Consts::CHUNK_SIZE_BLOCKS + 1;

	auto add_edge = [&](const PseudoChunkData::EdgeEntry &edge, glm::ivec3 add_coord) {
		glm::ivec3 cell_coord = add_coord + glm::ivec3(edge.x, edge.y, edge.z);
		if (cell_coord.x < -1 || cell_coord.y < -1 || cell_coord.z < -1) {
			return;
		}
		if (cell_coord.x > MAX_COORD || cell_coord.y > MAX_COORD || cell_coord.z > MAX_COORD) {
			return;
		}

		CellAccumulator &accum = cell_accumulator_map[getCellIndex(cell_coord)];

		glm::vec3 point = glm::vec3(cell_coord);
		point[edge.axis] += static_cast<float>(edge.offset_unorm) / 65535.0f;

		glm::vec3 normal;
		normal.x = static_cast<float>(edge.normal_x_snorm) / 32767.0f;
		normal.z = static_cast<float>(edge.normal_z_snorm) / 32767.0f;
		normal.y = (edge.normal_y_sign ? -1.0f : 1.0f) * std::sqrt(1.0f - normal.x * normal.x - normal.z * normal.z);

		accum.qef_solver.addPlane(point, normal);
	};

	auto add_edges = [&](std::span<const PseudoChunkData::EdgeEntry> entries, glm::ivec3 add_coord) {
		for (const auto &edge : entries) {
			add_edge(edge, add_coord);
		}
	};

	auto add_cell = [&](const PseudoChunkData::CellEntry &cell, glm::ivec3 add_coord) {
		glm::ivec3 cell_coord = add_coord + glm::ivec3(cell.x, cell.y, cell.z);
		if (cell_coord.x < -1 || cell_coord.y < -1 || cell_coord.z < -1) {
			return;
		}
		if (cell_coord.x > MAX_COORD || cell_coord.y > MAX_COORD || cell_coord.z > MAX_COORD) {
			return;
		}

		// Don't add material information to cells without surface-crossing edges.
		// This information might occur as a result of pseudo chunk data aggregation.
		auto iter = cell_accumulator_map.find(getCellIndex(cell_coord));
		if (iter != cell_accumulator_map.end()) {
			iter->second.materials = cell;
		}
	};

	auto add_cells = [&](std::span<const PseudoChunkData::CellEntry> entries, glm::ivec3 add_coord) {
		for (const auto &cell : entries) {
			add_cell(cell, add_coord);
		}
	};

	if (datas[0]->empty()) {
		return;
	}

	constexpr int32_t B = Consts::CHUNK_SIZE_BLOCKS;

	constexpr glm::ivec3 ADD_COORD_TABLE[21] = {
		glm::ivec3(0, 0, 0),

		glm::ivec3(+B, 0, 0),
		glm::ivec3(-B, 0, 0),
		glm::ivec3(0, +B, 0),
		glm::ivec3(0, -B, 0),
		glm::ivec3(0, 0, +B),
		glm::ivec3(0, 0, -B),

		// TODO: complete me for edge adjacents
		glm::ivec3(),
		glm::ivec3(),
		glm::ivec3(),
		glm::ivec3(),

		glm::ivec3(),
		glm::ivec3(),
		glm::ivec3(),
		glm::ivec3(),

		glm::ivec3(),
		glm::ivec3(),
		glm::ivec3(),
		glm::ivec3(),
	};

	for (size_t i = 0; i < 21; i++) {
		add_edges(datas[i]->edgeEntries(), ADD_COORD_TABLE[i]);
	}

	for (size_t i = 0; i < 21; i++) {
		add_cells(datas[i]->cellEntries(), ADD_COORD_TABLE[i]);
	}

	// TODO: resolve accumulators, generate geometry
}

} // namespace voxen::land
