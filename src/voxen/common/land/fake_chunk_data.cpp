#include <voxen/common/land/fake_chunk_data.hpp>

#include <voxen/common/land/land_chunk.hpp>
#include <voxen/common/land/land_public_consts.hpp>
#include <voxen/common/land/land_utils.hpp>

#include <unordered_map>
#include <vector>

namespace voxen::land
{

// `FakeFace::[x|y|z]` bit sizes depend on chunk size
static_assert(Consts::CHUNK_SIZE_BLOCKS == 32, "FakeChunkData is hardcoded for 32-block chunks");

static_assert(sizeof(FakeChunkData::FakeFace) == 8 + 4/*TODO:debug*/);

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

FakeChunkData::FakeChunkData(ChunkAdjacencyRef ref)
{
	constexpr static uint32_t N = Consts::CHUNK_SIZE_BLOCKS;

	// Allocate on heap, expanded array is pretty large
	auto expanded_ids = std::make_unique<CubeArray<uint16_t, N + 2>>();
	ref.expandBlockIds(expanded_ids->view());

	std::vector<FakeFace> faces;
	std::vector<PackedColorSrgb> colors;

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

		// Not setting color array indices. They are not necessary yet
		// as we're inserting faces and colors in lockstep. Will assign
		// them later after color deduplication.

		if (expanded_ids->load(x + 2, y + 1, z + 1) == 0) {
			// X+ face visible
			faces.emplace_back(x, y, z, 0, 0, FULL_CORNER_WEIGHTS);
			colors.emplace_back(FACE_COLOR_CODING[0]);
		}

		if (expanded_ids->load(x, y + 1, z + 1) == 0) {
			// X- face visible
			faces.emplace_back(x, y, z, 1, 0, FULL_CORNER_WEIGHTS);
			colors.emplace_back(FACE_COLOR_CODING[1]);
		}

		if (expanded_ids->load(x + 1, y + 2, z + 1) == 0) {
			// Y+ face visible
			faces.emplace_back(x, y, z, 2, 0, FULL_CORNER_WEIGHTS);
			colors.emplace_back(FACE_COLOR_CODING[2]);
		}

		if (expanded_ids->load(x + 1, y, z + 1) == 0) {
			// Y- face visible
			faces.emplace_back(x, y, z, 3, 0, FULL_CORNER_WEIGHTS);
			colors.emplace_back(FACE_COLOR_CODING[3]);
		}

		if (expanded_ids->load(x + 1, y + 1, z + 2) == 0) {
			// Z+ face visible
			faces.emplace_back(x, y, z, 4, 0, FULL_CORNER_WEIGHTS);
			colors.emplace_back(FACE_COLOR_CODING[4]);
		}

		if (expanded_ids->load(x + 1, y + 1, z) == 0) {
			// Z- face visible
			faces.emplace_back(x, y, z, 5, 0, FULL_CORNER_WEIGHTS);
			colors.emplace_back(FACE_COLOR_CODING[5]);
		}
	});

	assert(faces.size() == colors.size());

	// Deduplicate colors
	std::unordered_map<uint32_t, size_t> colors_dedup_map;
	std::vector<PackedColorSrgb> dedup_colors;

	for (size_t i = 0; i < faces.size(); i++) {
		auto [iter, inserted] = colors_dedup_map.try_emplace(colors[i].toUint32(), dedup_colors.size());

		if (inserted) {
			dedup_colors.emplace_back(colors[i]);
		}

		if (iter->second > (1 << 14)) {
			// Not enough color table index bits in `FakeFace`, need to reduce the palette.
			// TODO: implement color quantization pass.
		}

		faces[i].color_array_index = uint32_t(iter->second);
		faces[i].color_packed_srgb = colors[i].toUint32();
	}

	m_faces = extras::dyn_array(faces.begin(), faces.end());
	m_colors = extras::dyn_array(dedup_colors.begin(), dedup_colors.end());
}

FakeChunkData::FakeChunkData(std::span<const FakeChunkData *, 8> hires)
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
		const FakeChunkData *fcd = hires[i];
		if (!fcd) {
			continue;
		}

		for (const FakeFace &face : fcd->m_faces) {
			glm::ivec3 coord(face.x, face.y, face.z);
			coord += COORD_OFFSET[i];
			coord /= 2;

			int32_t packed_coord = coord.x * 256 * 256 + coord.y * 256 + coord.z;
			faces_combine[face.face_index][packed_coord] += PackedColorSrgb(face.color_packed_srgb).toVec4();
		}
	}

	size_t num_faces = 0;
	for (uint32_t face = 0; face < 6; face++) {
		num_faces += faces_combine[face].size();
	}

	m_faces = extras::dyn_array<FakeFace>(num_faces);

	size_t insert_pos = 0;
	for (uint32_t face = 0; face < 6; face++) {
		for (const auto &[key, value] : faces_combine[face]) {
			FakeFace &ff = m_faces[insert_pos++];

			ff.x = uint32_t(key / (256 * 256));
			ff.y = uint32_t(key % (256 * 256) / 256);
			ff.z = uint32_t(key % 256);
			ff.face_index = face;
			ff.color_packed_srgb = PackedColorSrgb(value / value.w).toUint32();
		}
	}
}

} // namespace voxen::land
