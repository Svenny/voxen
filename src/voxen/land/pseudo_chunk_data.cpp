#include <voxen/land/pseudo_chunk_data.hpp>

#include <voxen/land/land_chunk.hpp>
#include <voxen/land/land_public_consts.hpp>
#include <voxen/land/land_utils.hpp>

#include "land_geometry_utils_private.hpp"

#include <glm/gtc/packing.hpp>

namespace voxen::land
{

static_assert(sizeof(PseudoChunkData::CellEntry) == 24, "24-byte packing of PseudoChunkData::CellEntry is broken");

using detail::SurfaceMatHistEntry;

namespace
{

// f - fixed color flag              f rrrrr ggggg bbbbb
constexpr uint16_t FAKE_COLOR_XP = 0b1'11111'00000'00000;
constexpr uint16_t FAKE_COLOR_XM = 0b1'11111'11111'00000;
constexpr uint16_t FAKE_COLOR_YP = 0b1'00000'11111'00000;
constexpr uint16_t FAKE_COLOR_YM = 0b1'00000'11111'11111;
constexpr uint16_t FAKE_COLOR_ZP = 0b1'00000'00000'11111;
constexpr uint16_t FAKE_COLOR_ZM = 0b1'11111'00000'11111;

// Chunk-local coordinate space adjustments for finer LOD cells, see `generateFromFinerLod`
constexpr glm::vec3 FINER_SURFACE_POINT_ADJUSTMENT[8] = {
	glm::vec3(0.0f, 0.0f, 0.0f),
	glm::vec3(0.0f, 0.0f, 0.5f),
	glm::vec3(0.5f, 0.0f, 0.0f),
	glm::vec3(0.5f, 0.0f, 0.5f),
	glm::vec3(0.0f, 0.5f, 0.0f),
	glm::vec3(0.0f, 0.5f, 0.5f),
	glm::vec3(0.5f, 0.5f, 0.0f),
	glm::vec3(0.5f, 0.5f, 0.5f),
};

bool isBlockSolid(Chunk::BlockId block_id) noexcept
{
	return block_id != 0;
}

} // namespace

void PseudoChunkData::generateFromLod0(std::span<const Chunk *const, 27> chunks)
{
	constexpr uint32_t B = Consts::CHUNK_SIZE_BLOCKS;
	constexpr uint32_t B2 = B + B;

	// Stores two triples for each face/edge-adjacent chunk:
	// - XYZ to add when storing into expanded array ([0], [1], [2])
	// - XYZ limits of iteration over this chunk ([3], [4], [5])
	constexpr uint32_t ADJACENT_WALK_TABLE[18][6] = {
		// X face-adjacent chunks
		{ B2, 0, 0, 1, B, B },
		{ B2, 0, B, 1, B, B },
		{ B2, B, 0, 1, B, B },
		{ B2, B, B, 1, B, B },
		// Y face-adjacent chunks
		{ 0, B2, 0, B, 1, B },
		{ 0, B2, B, B, 1, B },
		{ B, B2, 0, B, 1, B },
		{ B, B2, B, B, 1, B },
		// Z face-adjacent chunks
		{ 0, 0, B2, B, B, 1 },
		{ B, 0, B2, B, B, 1 },
		{ 0, B, B2, B, B, 1 },
		{ B, B, B2, B, B, 1 },
		// X edge-adjacent chunks
		{ 0, B2, B2, B, 1, 1 },
		{ B, B2, B2, B, 1, 1 },
		// Y edge-adjacent chunks
		{ B2, 0, B2, 1, B, 1 },
		{ B2, B, B2, 1, B, 1 },
		// Z edge-adjacent chunks
		{ B2, B2, 0, 1, 1, B },
		{ B2, B2, B, 1, 1, B },
	};

	// Gather block IDs from all chunks into one huge array and
	// then walk over it. Not strictly necessary but the alternative
	// is to handle tons of special cases for different adjacency types.
	// Allocate on heap, expanded array is pretty large.
	// XXX: this thrashes caches like hell. Better to unpack layer by layer
	// while iterating in Y, we need only two (three?) layers at a time.
	// 3D array can easily annihilate L2 cache while 2D might even fit in L1.
	auto expanded_ids = std::make_unique<CubeArray<Chunk::BlockId, B2 + 1>>();

	// Expand "primary" chunks using the special unpack function
	for (size_t i = 0; i < 8; i++) {
		const Chunk *chunk = chunks[i];

		const uint32_t add_x = (i & 0b010) ? B : 0;
		const uint32_t add_y = (i & 0b100) ? B : 0;
		const uint32_t add_z = (i & 0b001) ? B : 0;

		chunk->blockIds().expand(expanded_ids->view<B>(glm::uvec3(add_x, add_y, add_z)));
	}

	// Other chunks need a few blocks, gather them directly.
	// XXX: this is the ultimate cache-thrasher 9000!
	for (size_t i = 0; i < 18; i++) {
		const auto &block_ids = chunks[i + 8]->blockIds();

		const uint32_t add_x = ADJACENT_WALK_TABLE[i][0];
		const uint32_t add_y = ADJACENT_WALK_TABLE[i][1];
		const uint32_t add_z = ADJACENT_WALK_TABLE[i][2];
		const uint32_t lim_x = ADJACENT_WALK_TABLE[i][3];
		const uint32_t lim_y = ADJACENT_WALK_TABLE[i][4];
		const uint32_t lim_z = ADJACENT_WALK_TABLE[i][5];

		// One or two of these loops will have only one iteration
		for (uint32_t y = 0; y < lim_y; y++) {
			for (uint32_t x = 0; x < lim_x; x++) {
				for (uint32_t z = 0; z < lim_z; z++) {
					expanded_ids->store(x + add_x, y + add_y, z + add_z, block_ids.load(x, y, z));
				}
			}
		}
	}

	// Vertex-adjacent chunks provides only one block ID
	expanded_ids->store(B2, B2, B2, chunks[26]->blockIds().load(0, 0, 0));

	std::vector<SurfaceMatHistEntry> material_histogram;
	glm::vec4 surface_point_weighted_sum;

	// Attempts to add an edge from 3x3x3 cell into the solver/histogram.
	// Don't forget to reset their states before moving to the next cell.
	auto try_add_edge = [&](Chunk::BlockId lower, Chunk::BlockId upper, glm::uvec3 edge_lower_coord_2x, int axis) {
		bool lower_solid = isBlockSolid(lower);
		bool upper_solid = isBlockSolid(upper);

		if (lower_solid == upper_solid) {
			return;
		}

		glm::uvec3 surface_point_coord_2x = edge_lower_coord_2x;
		// Surface point is at the "upper" end of the edge regardless of which block is solid
		surface_point_coord_2x[axis]++;

		glm::vec3 surface_point = glm::vec3(surface_point_coord_2x) * (0.5f / float(Consts::CHUNK_SIZE_BLOCKS));
		surface_point_weighted_sum += glm::vec4(surface_point, 1.0f);

		// TODO: select color/material from block interface
		uint16_t color = 0;

		switch (axis) {
		case 0:
			color = lower_solid ? FAKE_COLOR_XP : FAKE_COLOR_XM;
			break;
		case 1:
			color = lower_solid ? FAKE_COLOR_YP : FAKE_COLOR_YM;
			break;
		case 2:
			color = lower_solid ? FAKE_COLOR_ZP : FAKE_COLOR_ZM;
			break;
		}

		detail::GeometryUtils::addMatHistEntry(material_histogram, SurfaceMatHistEntry { color, 255 });
	};

	m_cell_entries.clear();

	// Now collect "Hermite data" by iterating over 3x3x3 cells.
	// Every such cell might produce one output cell.
	Utils::forYXZ<B>([&](uint32_t x, uint32_t y, uint32_t z) {
		const glm::uvec3 cell_base_coord_2x = 2u * glm::uvec3(x, y, z);

		// We have 3x3x3 block ID grid.
		// Only 8 corner blocks will determine if the generated cell crosses the surface.
		// But every block can still contribute to QEF solver and the material histogram.
		CubeArray<Chunk::BlockId, 3> cell_blocks;
		expanded_ids->extractTo<3>(cell_base_coord_2x, cell_blocks);

		uint8_t solid_mask = 0;
		solid_mask |= isBlockSolid(cell_blocks.data[0][0][0]) ? 0b00000001 : 0;
		solid_mask |= isBlockSolid(cell_blocks.data[0][0][2]) ? 0b00000010 : 0;
		solid_mask |= isBlockSolid(cell_blocks.data[0][2][0]) ? 0b00000100 : 0;
		solid_mask |= isBlockSolid(cell_blocks.data[0][2][2]) ? 0b00001000 : 0;
		solid_mask |= isBlockSolid(cell_blocks.data[2][0][0]) ? 0b00010000 : 0;
		solid_mask |= isBlockSolid(cell_blocks.data[2][0][2]) ? 0b00100000 : 0;
		solid_mask |= isBlockSolid(cell_blocks.data[2][2][0]) ? 0b01000000 : 0;
		solid_mask |= isBlockSolid(cell_blocks.data[2][2][2]) ? 0b10000000 : 0;

		if (solid_mask == 0 || solid_mask == 255) {
			// No surface crossing in this cell
			return;
		}

		// Reset states from previous cell aggregation
		material_histogram.clear();
		surface_point_weighted_sum = glm::vec4(0.0f);

		Utils::forYXZ<3>([&](uint32_t ax, uint32_t ay, uint32_t az) {
			const Chunk::BlockId lower_block = cell_blocks.data[ay][ax][az];
			const glm::uvec3 edge_lower_coord_2x = cell_base_coord_2x + glm::uvec3(ax, ay, az);

			if (ax + 1 < 3) {
				try_add_edge(lower_block, cell_blocks.data[ay][ax + 1][az], edge_lower_coord_2x, 0);
			}

			if (ay + 1 < 3) {
				try_add_edge(lower_block, cell_blocks.data[ay + 1][ax][az], edge_lower_coord_2x, 1);
			}

			if (az + 1 < 3) {
				try_add_edge(lower_block, cell_blocks.data[ay][ax][az + 1], edge_lower_coord_2x, 2);
			}
		});

		// We know that at least 3 edges were added, otherwise we'd have failed `solid_mask` check.
		// Because of `forYXZ` we will store produced entries already sorted in the required order.
		CellEntry &out_cell_entry = m_cell_entries.emplace_back();

		out_cell_entry.cell_index = glm::u8vec3(x, y, z);
		out_cell_entry.corner_solid_mask = solid_mask;

		detail::GeometryUtils::resolveMatHist(material_histogram, out_cell_entry);

		glm::vec3 surface_point = glm::vec3(surface_point_weighted_sum) / surface_point_weighted_sum.w;
		out_cell_entry.surface_point_unorm = glm::packUnorm<uint16_t>(surface_point);
		out_cell_entry.surface_point_sum_count = static_cast<uint16_t>(std::min(surface_point_weighted_sum.w, 65535.0f));
	});
}

void PseudoChunkData::generateFromFinerLod(std::span<const PseudoChunkData *const, 8> finer)
{
	constexpr uint32_t B = Consts::CHUNK_SIZE_BLOCKS;

	std::vector<SurfaceMatHistEntry> material_histogram;
	glm::vec4 surface_point_weighted_sum;

	// Iterate over all possible output cells, generating them as needed in order.
	// XXX: this is quite inefficient if finer resolution data has just a few cells filled.
	Utils::forYXZ<B>([&](uint32_t x, uint32_t y, uint32_t z) {
		const glm::uvec3 cell_base_coord_2x = 2u * glm::uvec3(x, y, z);

		// Aggregation of `corner_solid_mask`s of finer cells
		uint8_t solid_mask = 0;
		// Corner bits that are known exactly from found cells
		uint8_t known_mask = 0;

		// Reset states from previous cell aggregation
		material_histogram.clear();
		surface_point_weighted_sum = glm::vec4(0.0f);

		// Collect finer resolution data from 2x2x2 cells
		for (size_t i = 0; i < 8; i++) {
			glm::uvec3 search_coord = cell_base_coord_2x;
			if (i & 0b001) {
				search_coord.z++;
			}
			if (i & 0b010) {
				search_coord.x++;
			}
			if (i & 0b100) {
				search_coord.y++;
			}

			size_t finer_index = 0;
			if (search_coord.z >= B) {
				search_coord.z -= B;
				finer_index |= 0b001;
			}
			if (search_coord.x >= B) {
				search_coord.x -= B;
				finer_index |= 0b010;
			}
			if (search_coord.y >= B) {
				search_coord.y -= B;
				finer_index |= 0b100;
			}

			const CellEntry *finer_cell = finer[finer_index]->findEntry(search_coord);
			if (!finer_cell) {
				continue;
			}

			detail::GeometryUtils::addMatHistEntry(material_histogram, *finer_cell);

			glm::vec3 surface_point = glm::unpackUnorm<float>(finer_cell->surface_point_unorm);
			surface_point = surface_point * 0.5f + FINER_SURFACE_POINT_ADJUSTMENT[finer_index];

			float surface_point_weight = static_cast<float>(finer_cell->surface_point_sum_count);
			surface_point_weighted_sum += glm::vec4(surface_point * surface_point_weight, surface_point_weight);

			// i-th bit of solid mask is "owned" by i-th bit of i-th finer cell
			const uint32_t this_bit = 1u << i;
			solid_mask = (finer_cell->corner_solid_mask & this_bit) | (solid_mask & ~this_bit);
			known_mask |= this_bit;

			// Fill unknown solid mask bits. Note that conflicts (different finer
			// cells filling one unknown bit with different values) are impossible
			// if inputs are valid, e.g. all surface intersections at finer LOD grid
			// resolution were stored. This must be always true in our aggregation algorithm.
			solid_mask = (solid_mask & known_mask) | (finer_cell->corner_solid_mask & ~known_mask);
		}

		if (solid_mask == 0 || solid_mask == 255) {
			// No surface crossing in this cell
			return;
		}

		// We know that at least 3 edges were added, otherwise we'd have failed `solid_mask` check.
		// Because of `forYXZ` we will store produced entries already sorted in the required order.
		CellEntry &out_cell_entry = m_cell_entries.emplace_back();

		out_cell_entry.cell_index = glm::u8vec3(x, y, z);
		out_cell_entry.corner_solid_mask = solid_mask;

		detail::GeometryUtils::resolveMatHist(material_histogram, out_cell_entry);

		glm::vec3 surface_point = glm::vec3(surface_point_weighted_sum) / surface_point_weighted_sum.w;
		out_cell_entry.surface_point_unorm = glm::packUnorm<uint16_t>(surface_point);
		out_cell_entry.surface_point_sum_count = static_cast<uint16_t>(std::min(surface_point_weighted_sum.w, 65535.0f));
	});
}

const PseudoChunkData::CellEntry *PseudoChunkData::findEntry(glm::uvec3 cell_index) const noexcept
{
	auto projection = [](const CellEntry &e) -> uint32_t {
		// Note YXZ order - it conforms to cell iteration order
		return static_cast<uint32_t>((e.cell_index.y << 24) | (e.cell_index.x << 16) | e.cell_index.z);
	};

	const uint32_t target = (cell_index.y << 24) | (cell_index.x << 16) | cell_index.z;

	auto iter = std::ranges::lower_bound(m_cell_entries, target, std::less<uint32_t>(), projection);
	if (iter != m_cell_entries.end() && projection(*iter) == target) {
		return std::to_address(iter);
	}

	return nullptr;
}

} // namespace voxen::land
