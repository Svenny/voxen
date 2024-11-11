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

static_assert(sizeof(PseudoSurfaceVertex) == 16, "16-byte packing of PseudoSurfaceVertex is brokne");

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

PseudoChunkSurface::PseudoChunkSurface(std::span<const PseudoSurfaceVertex> vertices, std::span<const uint32_t> indices)
{
	// Must not be more than UINT16_MAX vertices as we are using 16-bit index
	if (vertices.size() > UINT16_MAX || indices.size() > UINT32_MAX) {
		throw Exception::fromError(VoxenErrc::DataTooLarge, "too many vertices or indices in PseudoChunkSurface");
	}

	m_vertices = extras::dyn_array(vertices.begin(), vertices.end());
	m_indices = extras::dyn_array<uint16_t>(indices.size());

	// TODO: could also do a meshlet partitioning here
	for (size_t i = 0; i < indices.size(); i++) {
		assert(indices[i] < vertices.size());
		m_indices[i] = static_cast<uint16_t>(indices[i]);
	}
}

PseudoChunkSurface PseudoChunkSurface::build(const PseudoChunkData &data,
	std::array<const PseudoChunkData *, 6> adjacent)
{
	// TODO: fixup corner vertex positions using adjacent chunks
	(void) adjacent;

	if (data.empty()) {
		return {};
	}

	std::unordered_map<int32_t, VertexState> vertex_state;
	std::vector<PseudoSurfaceVertex> vertex_buffer;
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
		state.vertex_index = static_cast<uint32_t>(vertex_buffer.size());
		vertex_buffer.emplace_back();
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
		auto &vertex = vertex_buffer[state.vertex_index];

		glm::vec3 pos_normalized = state.position * DIVISOR;
		vertex.position_x_unorm = static_cast<uint16_t>(pos_normalized.x * UINT16_MAX);
		vertex.position_y_unorm = static_cast<uint16_t>(pos_normalized.y * UINT16_MAX);
		vertex.position_z_unorm = static_cast<uint16_t>(pos_normalized.z * UINT16_MAX);

		glm::vec3 normal = glm::normalize(state.normal_accumulator);
		vertex.normal_x_snorm = static_cast<int16_t>(normal.x * INT16_MAX);
		vertex.normal_y_snorm = static_cast<int16_t>(normal.y * INT16_MAX);
		vertex.normal_z_snorm = static_cast<int16_t>(normal.z * INT16_MAX);

		glm::vec3 color = glm::vec3(state.color_accumulator) / state.color_accumulator.a;
		vertex.albedo_r_linear = static_cast<uint32_t>(color.r * ((1 << 11) - 1));
		vertex.albedo_g_linear = static_cast<uint32_t>(color.g * ((1 << 11) - 1));
		vertex.albedo_b_linear = static_cast<uint32_t>(color.r * ((1 << 10) - 1));
	}

	return PseudoChunkSurface(vertex_buffer, index_buffer);
}

} // namespace voxen::land
