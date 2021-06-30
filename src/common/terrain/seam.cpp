#include <voxen/common/terrain/seam.hpp>

#include <voxen/common/terrain/octree_tables.hpp>

#include <bit>
#include <cstring>

namespace voxen
{

using terrain::ChunkOctree;
using terrain::ChunkOctreeCell;
using terrain::ChunkOctreeLeaf;
using terrain::ChunkOctreeNodeBase;

void TerrainChunkSeamSet::clear() noexcept
{
	m_refs.clear();
}

struct ExtendContext {
	ChunkOctree &tree;
	const int64_t root_x, root_y, root_z, root_size;
};

static uint32_t extendRoot(ExtendContext &ctx, int64_t base_x, int64_t base_y, int64_t base_z,
                           int64_t size, int8_t depth)
{
	assert(depth <= 0);

	if (depth == 0) {
		return ctx.tree.baseRoot();
	}

	const int64_t child_size = size / 2;
	const int64_t mid_x = base_x + child_size;
	const int64_t mid_y = base_y + child_size;
	const int64_t mid_z = base_z + child_size;

	int64_t child_base_x = base_x;
	int64_t child_base_y = base_y;
	int64_t child_base_z = base_z;
	int child_id = 0;

	if (ctx.root_x >= mid_x) {
		child_base_x = mid_x;
		child_id |= 2;
	}
	if (ctx.root_y >= mid_y) {
		child_base_y = mid_y;
		child_id |= 4;
	}
	if (ctx.root_z >= mid_z) {
		child_base_z = mid_z;
		child_id |= 1;
	}

	uint32_t child_node = extendRoot(ctx, child_base_x, child_base_y, child_base_z, child_size, depth + 1);

	auto[cell_id, cell] = ctx.tree.allocCell(depth);
	cell->children_ids[child_id] = child_node;
	return cell_id;
}

struct CopyContext {
	ChunkOctree &dst_tree;

	const ChunkOctree &src_tree;
	const ChunkOctreeNodeBase *src_node;
	const int64_t src_x, src_y, src_z;
	int64_t src_size;

	const uintptr_t strategy_mask;
	const int64_t contact_x, contact_y, contact_z;

	glm::vec3 coord_adjust_offset;
	float coord_adjust_scale;
};

static uint32_t copySubtree(CopyContext &ctx, uint32_t dst_node,
                            int64_t base_x, int64_t base_y, int64_t base_z,
                            int64_t size, int8_t depth)
{
	assert(ctx.src_node);

	if (dst_node == ChunkOctree::INVALID_NODE_ID) {
		if (ctx.src_size == size && ctx.src_node->is_leaf) {
			dst_node = ctx.dst_tree.allocLeaf(depth).first;
		} else {
			dst_node = ctx.dst_tree.allocCell(depth).first;
		}
	}

	if (size > ctx.src_size) {
		// We have not reached source root yet, continue descending
		const int64_t child_size = size / 2;
		const int64_t mid_x = base_x + child_size;
		const int64_t mid_y = base_y + child_size;
		const int64_t mid_z = base_z + child_size;

		int64_t child_base_x = base_x;
		int64_t child_base_y = base_y;
		int64_t child_base_z = base_z;
		int child_id = 0;

		if (ctx.src_x >= mid_x) {
			child_base_x = mid_x;
			child_id |= 2;
		}
		if (ctx.src_y >= mid_y) {
			child_base_y = mid_y;
			child_id |= 4;
		}
		if (ctx.src_z >= mid_z) {
			child_base_z = mid_z;
			child_id |= 1;
		}

		ChunkOctreeCell *cell = ctx.dst_tree.idToPointer(dst_node)->castToCell();
		uint32_t child_node = cell->children_ids[child_id];
		child_node = copySubtree(ctx, child_node, child_base_x, child_base_y, child_base_z, child_size, depth + 1);
		// Pointer could have been invalidated after calling `copySubtree`, reobtain it
		cell = ctx.dst_tree.idToPointer(dst_node)->castToCell();
		cell->children_ids[child_id] = child_node;
		return dst_node;
	}

	if (ctx.src_node->is_leaf) {
		// We've entered a leaf, no more recursion is possible.
		// Copy data from source with proper coordinate offsets.
		const ChunkOctreeLeaf *src_leaf = ctx.src_node->castToLeaf();
		ChunkOctreeLeaf *dst_leaf = ctx.dst_tree.idToPointer(dst_node)->castToLeaf();
		dst_leaf->surface_vertex = src_leaf->surface_vertex * ctx.coord_adjust_scale + ctx.coord_adjust_offset;
		dst_leaf->surface_normal = src_leaf->surface_normal;
		dst_leaf->corners = src_leaf->corners;
		// No need to copy QEF state - it's not used by surface builder
		return dst_node;
	}

	// We've entered a cell, now apply filter to each of its
	// children to determine if we need to descend there
	const ChunkOctreeCell *src_cell = ctx.src_node->castToCell();
	const int64_t child_size = size / 2;
	const int64_t mid_x = base_x + child_size;
	const int64_t mid_y = base_y + child_size;
	const int64_t mid_z = base_z + child_size;
	for (int i = 0; i < 8; i++) {
		uint32_t src_child = src_cell->children_ids[i];
		if (src_child == ChunkOctree::INVALID_NODE_ID) {
			continue;
		}

		const int64_t child_base_x = ((i & 2) ? mid_x : base_x);
		const int64_t child_base_y = ((i & 4) ? mid_y : base_y);
		const int64_t child_base_z = ((i & 1) ? mid_z : base_z);

		uintptr_t contact_mask = 0;
		if (child_base_x == ctx.contact_x) {
			contact_mask |= 1;
		}
		if (child_base_y == ctx.contact_y) {
			contact_mask |= 2;
		}
		if (child_base_z == ctx.contact_z) {
			contact_mask |= 4;
		}

		if ((ctx.strategy_mask & contact_mask) != ctx.strategy_mask) {
			// Some of the coordinates required by mask
			// are not equal to the contact point ones
			continue;
		}

		ctx.src_node = ctx.src_tree.idToPointer(src_child);
		ctx.src_size = child_size;
		ChunkOctreeCell *cell = ctx.dst_tree.idToPointer(dst_node)->castToCell();
		uint32_t child_node = cell->children_ids[i];
		child_node = copySubtree(ctx, child_node, child_base_x, child_base_y, child_base_z, child_size, depth + 1);
		// Pointer could have been invalidated after calling `copySubtree`, reobtain it
		cell = ctx.dst_tree.idToPointer(dst_node)->castToCell();
		cell->children_ids[i] = child_node;
	}

	return dst_node;
}

void TerrainChunkSeamSet::extendOctree(TerrainChunkHeader header, ChunkOctree &output)
{
	const auto[new_scale, new_base_x, new_base_y, new_base_z] = selectExtendedRootDimensions(header);
	const int64_t new_root_size = int64_t(new_scale * terrain::Config::CHUNK_SIZE);
	const int8_t new_root_depth = -int8_t(std::countr_zero(new_scale / header.scale));

	ExtendContext ext_ctx {
		.tree = output,
		.root_x = header.base_x, .root_y = header.base_y, .root_z = header.base_z,
		.root_size = int64_t(header.scale * terrain::Config::CHUNK_SIZE)
	};
	output.setExtendedRoot(extendRoot(ext_ctx, new_base_x, new_base_y, new_base_z, new_root_size, new_root_depth));

	// These coords are maximal for unextended octree/chunk and represent chunks contact point
	const int64_t mid_x = header.base_x + int64_t(header.scale * terrain::Config::CHUNK_SIZE);
	const int64_t mid_y = header.base_y + int64_t(header.scale * terrain::Config::CHUNK_SIZE);
	const int64_t mid_z = header.base_z + int64_t(header.scale * terrain::Config::CHUNK_SIZE);

	// Now copy nodes from other trees
	for (uintptr_t ref : m_refs) {
		const TerrainChunk *src_chunk = reinterpret_cast<const TerrainChunk *>(ref & ~uintptr_t(0b111));
		const ChunkOctree &src_octree = src_chunk->secondaryData().octree;
		const TerrainChunkHeader &src_header = src_chunk->header();

		if (src_octree.baseRoot() == ChunkOctree::INVALID_NODE_ID) {
			// Skip emtry trees
			continue;
		}

		CopyContext ctx {
			.dst_tree = output,

			.src_tree = src_octree,
			.src_node = src_octree.idToPointer(src_octree.baseRoot()),
			.src_x = src_header.base_x, .src_y = src_header.base_y, .src_z = src_header.base_z,
			.src_size = int64_t(src_header.scale * terrain::Config::CHUNK_SIZE),

			.strategy_mask = ref & uintptr_t(0b111),
			.contact_x = mid_x, .contact_y = mid_y, .contact_z = mid_z,

			.coord_adjust_offset = glm::vec3(src_header.base_x - header.base_x,
			                                 src_header.base_y - header.base_y,
			                                 src_header.base_z - header.base_z) / float(header.scale),
			.coord_adjust_scale = float(src_header.scale) / float(header.scale)
		};
		copySubtree(ctx, output.extendedRoot(), new_base_x, new_base_y, new_base_z, new_root_size, new_root_depth);
	}
}

std::tuple<uint32_t, int64_t, int64_t, int64_t>
TerrainChunkSeamSet::selectExtendedRootDimensions(const TerrainChunkHeader &header) const
{
	// We'll always add at least one "above root" node
	uint32_t new_scale = header.scale * 2;
	// Minimal coordinates met in any of the refs
	// (not necessarily all in one ref at once)
	int64_t min_x = INT64_MAX, min_y = INT64_MAX, min_z = INT64_MAX;
	// Header of any ref with the maximal size
	const TerrainChunkHeader *pivot = &header;

	for (uintptr_t ref : m_refs) {
		const TerrainChunk *chunk = reinterpret_cast<const TerrainChunk *>(ref & ~uintptr_t(0b111));
		const TerrainChunkHeader &h = chunk->header();

		if (h.scale * 2 > new_scale) {
			new_scale = h.scale * 2;
			pivot = &h;
		}
		min_x = std::min(min_x, h.base_x);
		min_y = std::min(min_y, h.base_y);
		min_z = std::min(min_z, h.base_z);
	}

	const int64_t half_size = int64_t((new_scale / 2) * terrain::Config::CHUNK_SIZE);
	// Place new root base at the base of pivot first. Assuming refs set is valid, it
	// it should be sufficient to decrease each coordinate by half root size to obtain
	// a properly aligned extended octree (such that any ref is strictly its valid child).
	// NOTE: I don't have a rigorous proof of sufficiency. Check this logic first
	// if you meet cracks in the seams.
	int64_t base_x = pivot->base_x;
	int64_t base_y = pivot->base_y;
	int64_t base_z = pivot->base_z;
	if (min_x < base_x) {
		base_x -= half_size;
	}
	if (min_y < base_y) {
		base_y -= half_size;
	}
	if (min_z < base_z) {
		base_z -= half_size;
	}

	return { new_scale, base_x, base_y, base_z };
}

}
