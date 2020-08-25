#include <voxen/common/terrain/generator.hpp>

#include <voxen/util/log.hpp>

#include <cmath>

namespace voxen
{

void TerrainGenerator::generate(TerrainChunk &chunk)
{
	Log::trace("Generating chunk at ({}, {}, {})(x{})", chunk.baseX(), chunk.baseY(), chunk.baseZ(), chunk.scale());
	auto &output = chunk.data().voxel_id;

	const uint32_t step = chunk.scale();

	// TODO: this is a temporary stub, add real land generator
	for (uint32_t i = 0; i <= TerrainChunk::SIZE; i++) {
		int64_t y = chunk.baseY() + i * step;
		for (uint32_t j = 0; j <= TerrainChunk::SIZE; j++) {
			int64_t x = chunk.baseX() + j * step;
			for (uint32_t k = 0; k <= TerrainChunk::SIZE; k++) {
				int64_t z = chunk.baseZ() + k * step;

				uint8_t voxel = 0;
				if (y <= 0 || std::abs(x) > 100 || std::abs(z) > 100)
					voxel = 1;

				output[i][j][k] = voxel;
			}
		}
	}
}

}
