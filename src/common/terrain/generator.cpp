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
	auto &out_voxels = chunk.data().voxel_id;
	auto &out_values = chunk.data().value_id;
	auto &out_grads = chunk.data().gradient_id;

	const uint32_t step = header.scale;

	bool canEdit = chunk.beginEdit();
	if (!canEdit) {
		Log::error("Critical error on chunk creation: chunk can't be edited");
		return;
	}

	try {
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

					double dx = 0.05 * 5.0 * std::cos(0.05 * x);
					double dy = 1;
					double dz = - 0.05 * std::sin(0.05 * z);

					out_voxels[i][j][k] = voxel;
					out_values[i][j][k] = value;
					out_grads[i][j][k] = glm::dvec3(dx, dy, dz);
				}
			}
		}

		TerrainSurfaceBuilder::calcSurface(chunk.data(), chunk.data().surface);
	}
	catch (std::exception& e) {
		Log::error("Unexpected error on chunk creating process: {}", e.what());
	}

	chunk.endEdit();
}

}
