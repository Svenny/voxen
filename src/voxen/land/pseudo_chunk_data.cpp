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

static_assert(sizeof(PseudoChunkData::Face) == 12, "12-byte packing of PseudoChunkData::Face is broken");

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

constexpr uint32_t FULL_CORNER_WEIGHTS = UINT32_MAX;

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
			faces.emplace_back(x, y, z, 0, 0, FULL_CORNER_WEIGHTS, FACE_COLOR_CODING[0].toUint32());
		}

		if (expanded_ids->load(x, y + 1, z + 1) == 0) {
			// X- face visible
			faces.emplace_back(x, y, z, 1, 0, FULL_CORNER_WEIGHTS, FACE_COLOR_CODING[1].toUint32());
		}

		if (expanded_ids->load(x + 1, y + 2, z + 1) == 0) {
			// Y+ face visible
			faces.emplace_back(x, y, z, 2, 0, FULL_CORNER_WEIGHTS, FACE_COLOR_CODING[2].toUint32());
		}

		if (expanded_ids->load(x + 1, y, z + 1) == 0) {
			// Y- face visible
			faces.emplace_back(x, y, z, 3, 0, FULL_CORNER_WEIGHTS, FACE_COLOR_CODING[3].toUint32());
		}

		if (expanded_ids->load(x + 1, y + 1, z + 2) == 0) {
			// Z+ face visible
			faces.emplace_back(x, y, z, 4, 0, FULL_CORNER_WEIGHTS, FACE_COLOR_CODING[4].toUint32());
		}

		if (expanded_ids->load(x + 1, y + 1, z) == 0) {
			// Z- face visible
			faces.emplace_back(x, y, z, 5, 0, FULL_CORNER_WEIGHTS, FACE_COLOR_CODING[5].toUint32());
		}
	});

	m_faces = extras::dyn_array(faces.begin(), faces.end());
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

} // namespace voxen::land
