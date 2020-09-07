#include <voxen/common/terrain/generator.hpp>

#include <voxen/util/log.hpp>

#include <cmath>

namespace voxen
{

void TerrainGenerator::generate(TerrainChunk &chunk)
{
	constexpr uint32_t SIZE = TerrainChunk::SIZE;

	const TerrainChunkHeader& header = chunk.header();

	Log::trace("Generating chunk at ({}, {}, {})(x{})", header.base_x, header.base_y, header.base_z, header.scale);
	auto &output = chunk.data().voxel_id;

	const uint32_t step = header.scale;

	// TODO: this is a temporary stub, add real land generator
	for (uint32_t i = 0; i < SIZE; i++) {
		int64_t y = header.base_y + i * step;
		for (uint32_t j = 0; j < SIZE; j++) {
			int64_t x = header.base_x + j * step;
			for (uint32_t k = 0; k < SIZE; k++) {
				int64_t z = header.base_z + k * step;

				uint8_t voxel = 0;
				double value = double(y) + 5.0 * std::sin(0.05 * double(x + z));
				if (value <= 0.0)
					voxel = 1;

				output[i][j][k] = voxel;
			}
		}
	}

	// TODO: this is a temporary cuberille surface generator, implement
	// proper isosurface extraction algorithm and factor it out
	auto &surface = chunk.data().surface;
	for (uint32_t i = 0; i < SIZE; i++) {
		for (uint32_t j = 0; j < SIZE; j++) {
			for (uint32_t k = 0; k < SIZE; k++) {
				bool is_cur_zero = (output[i][j][k] == 0);
				if (is_cur_zero) {
					if (k + 1 < SIZE && output[i][j][k + 1] != 0) {
						// Z face
						uint32_t i0 = surface.addVertex({ glm::vec3(i + 0, j + 0, k + 1), glm::vec3(0, 0, -1) });
						uint32_t i1 = surface.addVertex({ glm::vec3(i + 0, j + 1, k + 1), glm::vec3(0, 0, -1) });
						uint32_t i2 = surface.addVertex({ glm::vec3(i + 1, j + 1, k + 1), glm::vec3(0, 0, -1) });
						uint32_t i3 = surface.addVertex({ glm::vec3(i + 1, j + 0, k + 1), glm::vec3(0, 0, -1) });
						surface.addTriangle(i0, i2, i1);
						surface.addTriangle(i0, i3, i2);
					}
					if (j + 1 < SIZE && output[i][j + 1][k] != 0) {
						// X face
						uint32_t i0 = surface.addVertex({ glm::vec3(i + 0, j + 1, k + 0), glm::vec3(-1, 0, 0) });
						uint32_t i1 = surface.addVertex({ glm::vec3(i + 1, j + 1, k + 0), glm::vec3(-1, 0, 0) });
						uint32_t i2 = surface.addVertex({ glm::vec3(i + 1, j + 1, k + 1), glm::vec3(-1, 0, 0) });
						uint32_t i3 = surface.addVertex({ glm::vec3(i + 0, j + 1, k + 1), glm::vec3(-1, 0, 0) });
						surface.addTriangle(i0, i2, i1);
						surface.addTriangle(i0, i3, i2);
					}
					if (i + 1 < SIZE && output[i + 1][j][k] != 0) {
						// Y face
						uint32_t i0 = surface.addVertex({ glm::vec3(i + 1, j + 0, k + 0), glm::vec3(0, -1, 0) });
						uint32_t i1 = surface.addVertex({ glm::vec3(i + 1, j + 0, k + 1), glm::vec3(0, -1, 0) });
						uint32_t i2 = surface.addVertex({ glm::vec3(i + 1, j + 1, k + 1), glm::vec3(0, -1, 0) });
						uint32_t i3 = surface.addVertex({ glm::vec3(i + 1, j + 1, k + 0), glm::vec3(0, -1, 0) });
						surface.addTriangle(i0, i2, i1);
						surface.addTriangle(i0, i3, i2);
					}
				} else {
					if (k + 1 < SIZE && output[i][j][k + 1] == 0) {
						// Z face
						uint32_t i0 = surface.addVertex({ glm::vec3(i + 0, j + 0, k + 1), glm::vec3(0, 0, 1) });
						uint32_t i1 = surface.addVertex({ glm::vec3(i + 0, j + 1, k + 1), glm::vec3(0, 0, 1) });
						uint32_t i2 = surface.addVertex({ glm::vec3(i + 1, j + 1, k + 1), glm::vec3(0, 0, 1) });
						uint32_t i3 = surface.addVertex({ glm::vec3(i + 1, j + 0, k + 1), glm::vec3(0, 0, 1) });
						surface.addTriangle(i0, i1, i2);
						surface.addTriangle(i0, i2, i3);
					}
					if (j + 1 < SIZE && output[i][j + 1][k] == 0) {
						// X face
						uint32_t i0 = surface.addVertex({ glm::vec3(i + 0, j + 1, k + 0), glm::vec3(1, 0, 0) });
						uint32_t i1 = surface.addVertex({ glm::vec3(i + 1, j + 1, k + 0), glm::vec3(1, 0, 0) });
						uint32_t i2 = surface.addVertex({ glm::vec3(i + 1, j + 1, k + 1), glm::vec3(1, 0, 0) });
						uint32_t i3 = surface.addVertex({ glm::vec3(i + 0, j + 1, k + 1), glm::vec3(1, 0, 0) });
						surface.addTriangle(i0, i1, i2);
						surface.addTriangle(i0, i2, i3);
					}
					if (i + 1 < SIZE && output[i + 1][j][k] == 0) {
						// Y face
						uint32_t i0 = surface.addVertex({ glm::vec3(i + 1, j + 0, k + 0), glm::vec3(0, 1, 0) });
						uint32_t i1 = surface.addVertex({ glm::vec3(i + 1, j + 0, k + 1), glm::vec3(0, 1, 0) });
						uint32_t i2 = surface.addVertex({ glm::vec3(i + 1, j + 1, k + 1), glm::vec3(0, 1, 0) });
						uint32_t i3 = surface.addVertex({ glm::vec3(i + 1, j + 1, k + 0), glm::vec3(0, 1, 0) });
						surface.addTriangle(i0, i1, i2);
						surface.addTriangle(i0, i2, i3);
					}
				}
			}
		}
	}
}

}
