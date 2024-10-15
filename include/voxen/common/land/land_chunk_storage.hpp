#pragma once

#include <voxen/common/land/cube_array.hpp>
#include <voxen/common/land/land_public_consts.hpp>

#include <memory>

namespace voxen::land
{

template<typename T>
class CompressedChunkStorage {
public:
	using ExpandedArray = CubeArray<T, Consts::CHUNK_SIZE_BLOCKS>;

	CompressedChunkStorage() = default;
	explicit CompressedChunkStorage(const ExpandedArray &expanded);
	CompressedChunkStorage(CompressedChunkStorage &&) noexcept;
	CompressedChunkStorage(const CompressedChunkStorage &);
	CompressedChunkStorage &operator=(CompressedChunkStorage &&) noexcept;
	CompressedChunkStorage &operator=(const CompressedChunkStorage &) = delete; // Not implemented yet
	~CompressedChunkStorage() = default;

	void expand(CubeArrayView<T, Consts::CHUNK_SIZE_BLOCKS> view) const noexcept;

	void setUniform(T value) noexcept;

	bool uniform() const noexcept { return !m_nodes; }

	T load(uint32_t x, uint32_t y, uint32_t z) const noexcept;
	T operator[](glm::uvec3 pos) const noexcept { return load(pos.x, pos.y, pos.z); }

private:
	struct Leaf {
		T data[8];
	};

	struct Node {
		union {
			uint64_t nonuniform_leaf_mask = 0;
			T uniform_value;
		};
		std::unique_ptr<Leaf[]> leaves;

		bool uniform() const noexcept { return !leaves; }
	};

	union {
		uint64_t m_nonzero_node_mask = 0;
		T m_uniform_value;
	};
	std::unique_ptr<Node[]> m_nodes;
};

template<>
class CompressedChunkStorage<bool> {
public:
	using ExpandedArray = CubeArray<bool, Consts::CHUNK_SIZE_BLOCKS>;

	CompressedChunkStorage() = default;
	explicit CompressedChunkStorage(const ExpandedArray &expanded);
	CompressedChunkStorage(CompressedChunkStorage &&other) noexcept;
	CompressedChunkStorage(const CompressedChunkStorage &);
	CompressedChunkStorage &operator=(CompressedChunkStorage &&other) noexcept;
	CompressedChunkStorage &operator=(const CompressedChunkStorage &) = delete; // Not implemented yet
	~CompressedChunkStorage() = default;

	void expand(ExpandedArray &expanded) const noexcept;

	void setUniform(bool value) noexcept;

private:
	struct Node {
		uint8_t m_leaf_mask[64];
	};

	uint64_t m_nonuniform_node_mask = 0;
	uint64_t m_uniform_value_mask = 0;
	std::unique_ptr<Node[]> m_nodes;
};

extern template class CompressedChunkStorage<uint8_t>;
extern template class CompressedChunkStorage<uint16_t>;
extern template class CompressedChunkStorage<uint32_t>;

} // namespace voxen::land
