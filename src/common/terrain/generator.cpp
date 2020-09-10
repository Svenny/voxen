#include <voxen/common/terrain/generator.hpp>
#include <voxen/common/terrain/surface_builder.hpp>

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
		double y = double(header.base_y + i * step) + 0.5;
		for (uint32_t j = 0; j < SIZE; j++) {
			double x = double(header.base_x + j * step) + 0.5;
			for (uint32_t k = 0; k < SIZE; k++) {
				double z = double(header.base_z + k * step) + 0.5;

				uint8_t voxel = 0;
				double value = y + 5.0 * (std::sin(0.05 * x) + std::cos(0.05 * z));

				//double value = (x * x + y * y + z * z) - 45000.0;
				if (value <= 0.0)
					voxel = 1;

				output[i][j][k] = voxel;
			}
		}
	}

	TerrainSurfaceBuilder::calcSurface(output, chunk.data().surface);
}

}
