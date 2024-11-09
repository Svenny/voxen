#include <voxen/land/compressed_chunk_storage.hpp>

#include <cstring>
#include <utility>

namespace voxen::land
{

namespace
{

static_assert(Consts::CHUNK_SIZE_BLOCKS == 32, "CompressedChunkStorage is hardcoded for 32-chunks");

glm::uvec3 nodeBaseOffset(uint32_t i) noexcept
{
	// Node bases are aligned to 8 blocks (3 binary zeros)
	glm::uvec3 res;
	res.x = (i & 0b001100) << 1;
	res.y = (i & 0b110000) >> 1;
	res.z = (i & 0b000011) << 3;
	return res;
}

glm::uvec3 leafBaseOffset(glm::uvec3 base, uint32_t i) noexcept
{
	// Leaf bases are aligned to 2 blocks (1 binary zero)
	base.x += (i & 0b001100) >> 1;
	base.y += (i & 0b110000) >> 3;
	base.z += (i & 0b000011) << 1;
	return base;
}

} // namespace

template<typename T>
CompressedChunkStorage<T>::CompressedChunkStorage(ConstExpandedView expanded)
{
	auto construct_node = [&](uint32_t index, Node &output) {
		const glm::uvec3 node_base = nodeBaseOffset(index);

		Leaf leaves[64];

		uint64_t nonuniform_leaf_mask = 0;

		T node_uniform_value = 0;
		bool met_uniform_leaf = false;
		bool whole_node_uniform = true;

		for (uint32_t i = 0; i < 64; i++) {
			// Gather leaf values
			CubeArray<T, 2> leaf_cube;
			expanded.extractTo(leafBaseOffset(node_base, i), leaf_cube);

			auto &leaf = leaves[i];
			// Well...
			leaf = std::bit_cast<Leaf>(leaf_cube);

			bool uniform = true;
			for (uint32_t j = 1; j < std::size(leaf.data); j++) {
				if (leaf.data[j] != leaf.data[0]) {
					uniform = false;
					break;
				}
			}

			if (!uniform) {
				// Non-uniform leaf
				nonuniform_leaf_mask |= uint64_t(1) << i;
				// Whole-node uniform optimization reuses mask bits which are now needed
				whole_node_uniform = false;
			} else if (!met_uniform_leaf) {
				// The first uniform leaf, set the value
				node_uniform_value = leaf.data[0];
				met_uniform_leaf = true;
			} else if (node_uniform_value != leaf.data[0]) {
				// Several different uniform values, disable it
				whole_node_uniform = false;
			}
		}

		if (whole_node_uniform && node_uniform_value == 0) {
			// Whole node is zero, don't construct it at all
			return false;
		}

		if (whole_node_uniform) {
			// Whole node is non-zero uniform, construct it without leaf allocation
			output.uniform_value = node_uniform_value;
			return true;
		}

		// Non-uniform node, allocate leaves + single uniform values.
		// Leaf has 8 entries so we can pack 8 uniform leaves in one.
		auto num_nonuniform_leaves = uint32_t(std::popcount(nonuniform_leaf_mask));
		auto num_uniform_leaves = (64 - num_nonuniform_leaves + 7) / 8;

		output.nonuniform_leaf_mask = nonuniform_leaf_mask;
		output.leaves = std::make_unique<Leaf[]>(num_nonuniform_leaves + num_uniform_leaves);

		Leaf *output_nonuniform = output.leaves.get();
		T *output_uniform = output.leaves[num_nonuniform_leaves].data;

		for (uint32_t i = 0; i < 64; i++) {
			if (nonuniform_leaf_mask & (uint64_t(1) << i)) {
				*output_nonuniform = leaves[i];
				output_nonuniform++;
			} else {
				*output_uniform = leaves[i].data[0];
				output_uniform++;
			}
		}

		return true;
	};

	Node nodes[64];
	uint32_t used_nodes = 0;

	T chunk_uniform_value = 0;
	bool met_uniform_node = false;
	bool whole_chunk_uniform = true;

	for (uint32_t i = 0; i < 64; i++) {
		Node &node = nodes[used_nodes];

		if (!construct_node(i, node)) {
			// Zero node

			met_uniform_node = true;
			if (chunk_uniform_value != 0) {
				// There was a non-zero uniform node
				whole_chunk_uniform = false;
			}

			continue;
		}

		if (!node.uniform()) {
			// Whole-chunk uniform optimization reuses mask bits which are now needed
			whole_chunk_uniform = false;
		} else if (!met_uniform_node) {
			// The first uniform node, set the value
			chunk_uniform_value = node.uniform_value;
		} else if (chunk_uniform_value != node.uniform_value) {
			// Several different uniform values, disable it
			whole_chunk_uniform = false;
		}

		m_nonzero_node_mask |= uint64_t(1) << i;
		used_nodes++;
	}

	if (whole_chunk_uniform) {
		// The whole chunk has uniform value, don't allocate nodes.
		// We overwrite nonzero node mask but it's irrelevant now.
		m_uniform_value = chunk_uniform_value;
		return;
	}

	// Allocatae nodes, nonzero node mask is already written
	m_nodes = std::make_unique<Node[]>(used_nodes);
	std::move(nodes, nodes + used_nodes, m_nodes.get());
}

template<typename T>
CompressedChunkStorage<T>::CompressedChunkStorage(CompressedChunkStorage &&other) noexcept
	: m_nodes(std::move(other.m_nodes))
{
	if (m_nodes) {
		m_nonzero_node_mask = std::exchange(other.m_nonzero_node_mask, 0);
	} else {
		m_uniform_value = std::exchange(other.m_uniform_value, 0);
	}
}

template<typename T>
CompressedChunkStorage<T>::CompressedChunkStorage(const CompressedChunkStorage &other)
{
	if (!other.m_nodes) {
		m_uniform_value = other.m_uniform_value;
		return;
	}

	const uint32_t num_nodes = uint32_t(std::popcount(other.m_nonzero_node_mask));

	m_nonzero_node_mask = other.m_nonzero_node_mask;
	m_nodes = std::make_unique<Node[]>(num_nodes);

	for (uint32_t i = 0; i < num_nodes; i++) {
		Node &node = m_nodes[i];
		const Node &other_node = other.m_nodes[i];

		if (other_node.uniform()) {
			node.uniform_value = other_node.uniform_value;
			continue;
		}

		uint32_t num_leaves = uint32_t(std::popcount(other_node.nonuniform_leaf_mask));
		// Add single uniform values
		num_leaves += (64 - num_leaves + 7) / 8;

		node.nonuniform_leaf_mask = other_node.nonuniform_leaf_mask;
		node.leaves = std::make_unique<Leaf[]>(num_leaves);
		std::copy(node.leaves.get(), node.leaves.get() + num_leaves, other_node.leaves.get());
	}
}

template<typename T>
CompressedChunkStorage<T> &CompressedChunkStorage<T>::operator=(CompressedChunkStorage &&other) noexcept
{
	// If `sizeof(T) > sizeof(uint64_t)` we'll have to handle whether
	// `m_nonzero_node_mask` or `m_uniform_value` is active in the union.
	// Otherwise we can just swap the larger one.
	static_assert(sizeof(T) <= sizeof(uint64_t));

	std::swap(m_nonzero_node_mask, other.m_nonzero_node_mask);
	std::swap(m_nodes, other.m_nodes);
	return *this;
}

template<typename T>
CompressedChunkStorage<T> &CompressedChunkStorage<T>::operator=(const CompressedChunkStorage &other)
{
	*this = CompressedChunkStorage(other);
	return *this;
}

template<typename T>
void CompressedChunkStorage<T>::expand(CubeArrayView<T, Consts::CHUNK_SIZE_BLOCKS> view) const noexcept
{
	if (!m_nodes) {
		// No nodes - the whole chunk is uniform
		view.fill(m_uniform_value);
		return;
	}

	const Node *node = m_nodes.get();

	for (uint32_t i = 0; i < 64; i++) {
		glm::uvec3 base = nodeBaseOffset(i);

		auto out_node_view = view.template view<8>(base);

		if (!(m_nonzero_node_mask & (uint64_t(1) << i))) {
			out_node_view.fill(0);
			continue;
		}

		if (node->uniform()) {
			out_node_view.fill(node->uniform_value);
			node++;
			continue;
		}

		const uint64_t nonuniform_mask = node->nonuniform_leaf_mask;
		const Leaf *nonuniform_leaf = node->leaves.get();
		const T *uniform_leaf = (nonuniform_leaf + std::popcount(nonuniform_mask))->data;

		for (uint32_t j = 0; j < 64; j++) {
			glm::uvec3 leaf_base = leafBaseOffset(base, j);

			auto out_leaf_view = view.template view<2>(leaf_base);

			if (nonuniform_mask & (uint64_t(1) << j)) {
				// Well...
				auto leaf_cube = std::bit_cast<CubeArray<T, 2>>(*nonuniform_leaf);
				out_leaf_view.fillFrom(leaf_cube.cview());
				nonuniform_leaf++;
			} else {
				out_leaf_view.fill(*uniform_leaf);
				uniform_leaf++;
			}
		}

		node++;
	}
}

template<typename T>
void CompressedChunkStorage<T>::setUniform(T value) noexcept
{
	m_nodes.reset();
	m_uniform_value = value;
}

template<typename T>
T CompressedChunkStorage<T>::load(uint32_t x, uint32_t y, uint32_t z) const noexcept
{
	if (!m_nodes) {
		return m_uniform_value;
	}

	uint32_t node_id = z / 8 + (x / 8) * 4 + (y / 8) * 16;

	if (!(m_nonzero_node_mask & (uint64_t(1) << node_id))) {
		return 0;
	}

	uint64_t node_bit = uint64_t(1) << node_id;
	uint64_t node_tail_mask = node_bit - 1;

	uint32_t array_index = uint32_t(std::popcount(m_nonzero_node_mask & node_tail_mask));

	const Node &node = m_nodes[array_index];
	if (node.uniform()) {
		return node.uniform_value;
	}

	uint32_t leaf_id = z % 8 / 2 + (x % 8 / 2) * 4 + (y % 8 / 2) * 16;

	uint64_t leaf_bit = uint64_t(1) << leaf_id;
	uint64_t leaf_tail_mask = leaf_bit - 1;

	if (node.nonuniform_leaf_mask & leaf_bit) {
		// Non-uniform leaf, skip past previous non-uniform ones
		array_index = uint32_t(std::popcount(node.nonuniform_leaf_mask & leaf_tail_mask));
		uint32_t element_id = z % 2 + (x % 2) * 2 + (y % 2) * 4;
		return node.leaves[array_index].data[element_id];
	}

	// Uniform leaf - skip past all non-uniform leaves
	array_index = uint32_t(std::popcount(node.nonuniform_leaf_mask));
	const T *element = node.leaves[array_index].data;
	// Invert non-uniform mask to get uniform leaves mask, skip past previous ones
	return *(element + std::popcount(~node.nonuniform_leaf_mask & leaf_tail_mask));
}

CompressedChunkStorage<bool>::CompressedChunkStorage(ConstExpandedView expanded)
{
	Node nodes[64];
	uint32_t used_nodes = 0;

	for (uint32_t i = 0; i < 64; i++) {
		CubeArray<bool, 8> node_bools;
		expanded.extractTo(nodeBaseOffset(i), node_bools);

		Node &node = nodes[used_nodes];
		// Reset all bits to zero so we won't need to clear them one by one
		memset(&node, 0, sizeof(Node));

		bool has_false = false;
		bool has_true = false;

		const bool *bools = node_bools.begin();
		for (size_t j = 0; j < node_bools.size(); j++) {
			// TODO: optimize: load 8 bools at once (uint64_t) -> pack first bits of bytes together
			if (bools[j]) {
				node.m_leaf_mask[j / 8] |= 1u << (j % 8);
				has_true = true;
			} else {
				has_false = true;
			}
		}

		if (has_false && has_true) {
			// Non-uniform node
			m_nonuniform_node_mask |= uint64_t(1) << i;
			used_nodes++;
		} else if (has_true) {
			// Uniform ones node, set its bit, don't store
			m_uniform_value_mask |= uint64_t(1) << i;
		} // else - uniform zeros node, do nothing
	}

	if (used_nodes > 0) {
		// Allocate nodes, masks are already written
		m_nodes = std::make_unique<Node[]>(used_nodes);
		std::move(nodes, nodes + used_nodes, m_nodes.get());
	}
}

CompressedChunkStorage<bool>::CompressedChunkStorage(CompressedChunkStorage &&other) noexcept
	: m_nodes(std::move(other.m_nodes))
{
	m_nonuniform_node_mask = std::exchange(other.m_nonuniform_node_mask, 0);
	m_uniform_value_mask = std::exchange(other.m_uniform_value_mask, 0);
}

CompressedChunkStorage<bool>::CompressedChunkStorage(const CompressedChunkStorage &other)
	: m_nonuniform_node_mask(other.m_nonuniform_node_mask), m_uniform_value_mask(other.m_uniform_value_mask)
{
	if (!other.m_nodes) {
		return;
	}

	const auto num_nodes = uint32_t(std::popcount(m_nonuniform_node_mask));

	m_nodes = std::make_unique<Node[]>(num_nodes);
	std::copy_n(other.m_nodes.get(), num_nodes, m_nodes.get());
}

CompressedChunkStorage<bool> &CompressedChunkStorage<bool>::operator=(CompressedChunkStorage &&other) noexcept
{
	std::swap(m_nonuniform_node_mask, other.m_nonuniform_node_mask);
	std::swap(m_uniform_value_mask, other.m_uniform_value_mask);
	std::swap(m_nodes, other.m_nodes);
	return *this;
}

CompressedChunkStorage<bool> &CompressedChunkStorage<bool>::operator=(const CompressedChunkStorage &other)
{
	*this = CompressedChunkStorage(other);
	return *this;
}

void CompressedChunkStorage<bool>::expand(ExpandedView expanded) const noexcept
{
	const Node *node = m_nodes.get();

	for (uint32_t i = 0; i < 64; i++) {
		glm::uvec3 base = nodeBaseOffset(i);
		uint64_t i_bit = uint64_t(1) << i;

		if (!(m_nonuniform_node_mask & i_bit)) {
			expanded.fill(base, glm::uvec3(8), !!(m_uniform_value_mask & i_bit));
			continue;
		}

		uint32_t j = 0;
		for (uint32_t y = base.y; y <= base.y + 8; y++) {
			for (uint32_t x = base.x; x <= base.x + 8; x++) {
				// TODO: optimize: uint64_t(mask) -> spread bits to bytes -> store 8 bools at once
				for (uint32_t z = base.z; z <= base.z + 8; z++) {
					expanded[glm::uvec3(x, y, z)] = !!(node->m_leaf_mask[j / 8] & (1u << j));
					j++;
				}
			}
		}
	}
}

void CompressedChunkStorage<bool>::setUniform(bool value) noexcept
{
	m_nodes.reset();
	m_nonuniform_node_mask = 0;
	m_uniform_value_mask = value ? ~uint64_t(0) : 0;
}

bool CompressedChunkStorage<bool>::load(uint32_t x, uint32_t y, uint32_t z) const noexcept
{
	uint32_t node_id = z / 8 + (x / 8) * 4 + (y / 8) * 16;

	uint64_t node_bit = uint64_t(1) << node_id;
	uint64_t node_tail_mask = node_bit - 1;

	if (!m_nodes) {
		return !!(m_uniform_value_mask & node_bit);
	}

	uint32_t array_index = uint32_t(std::popcount(m_nonuniform_node_mask & node_tail_mask));
	const Node &node = m_nodes[array_index];

	uint32_t leaf_id = z % 8 / 2 + (x % 8 / 2) * 4 + (y % 8 / 2) * 16;
	uint32_t leaf_bit_id = z % 2 + (x % 2) * 2 + (y % 2) * 4;

	return !!(node.m_leaf_mask[leaf_id] & (1u << leaf_bit_id));
}

template class VOXEN_API CompressedChunkStorage<uint8_t>;
template class VOXEN_API CompressedChunkStorage<uint16_t>;
template class VOXEN_API CompressedChunkStorage<uint32_t>;

} // namespace voxen::land
