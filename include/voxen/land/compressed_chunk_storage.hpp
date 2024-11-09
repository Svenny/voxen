#pragma once

#include <voxen/land/cube_array.hpp>
#include <voxen/land/land_public_consts.hpp>
#include <voxen/visibility.hpp>

#include <memory>

namespace voxen::land
{

// Compressed sparse octree-like storage for values in a chunk.
// Can eliminate uniform zero 8x8x8 subchunks and compress
// uniform 2x2x2 pieces into one value. If the whole chunk is
// uniform, can compress it in a single value too.
//
// Has a fixed 16 bytes overhead for the whole chunk
// and 16 bytes for each non-zero 8x8x8 subchunk.
//
// Designed mainly for long-term in-memory storage, offering
// some balance between access speed and compression ratio.
// It is advised to decompress it into a plain 3D array
// using `expand()` before doing complex operations on
// the chunk (when accessing more than a few values).
//
// Modifying the storage is not supported - you should decompress,
// change the plain 3D array and then compress it again.
//
// Template is instantiated only for uint8_t, uint16_t and uint32_t values.
// There is also a specialization for bool values, see below.
//
// Hardcoded for `Consts::CHUNK_SIZE_BLOCKS == 32`,
// changing it will require rewriting this class.
template<typename T>
class VOXEN_API CompressedChunkStorage {
public:
	using ConstExpandedView = CubeArrayView<const T, Consts::CHUNK_SIZE_BLOCKS>;
	using ExpandedView = CubeArrayView<T, Consts::CHUNK_SIZE_BLOCKS>;

	// Default constructor, initializes all values to zeros
	CompressedChunkStorage() = default;
	// Compress a plain 3D array
	explicit CompressedChunkStorage(ConstExpandedView expanded);
	CompressedChunkStorage(CompressedChunkStorage &&) noexcept;
	CompressedChunkStorage(const CompressedChunkStorage &);
	CompressedChunkStorage &operator=(CompressedChunkStorage &&) noexcept;
	CompressedChunkStorage &operator=(const CompressedChunkStorage &);
	~CompressedChunkStorage() = default;

	// Decompress into a plain 3D array
	void expand(ExpandedView view) const noexcept;

	// Set all values in the chunk to `value`
	void setUniform(T value) noexcept;

	// True if all values in the chunk are equal
	bool uniform() const noexcept { return !m_nodes; }

	// Single element access. Behavior is undefined if
	// any of x, y or z is out of chunk boundaries.
	//
	// Access is not particularly fast, use `expand()`
	// if you plan to access many values at once.
	T load(uint32_t x, uint32_t y, uint32_t z) const noexcept;
	// Same as `load(pos.x, pos.y, pos.z)`
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

// Specialization of `CompressedChunkStorage` for boolean values,
// offers even more compact storage. See the main template description.
//
// Has a fixed 24 bytes overhead and allocates storage
// only for non-uniform 8x8x8 subchunks as 512-bit masks.
//
// Just like the main template, hardcoded for chunk size of 32.
template<>
class VOXEN_API CompressedChunkStorage<bool> {
public:
	using ConstExpandedView = CubeArrayView<const bool, Consts::CHUNK_SIZE_BLOCKS>;
	using ExpandedView = CubeArrayView<bool, Consts::CHUNK_SIZE_BLOCKS>;

	// Default constructor, initializes all values to false
	CompressedChunkStorage() = default;
	// Compress a plain array
	explicit CompressedChunkStorage(ConstExpandedView expanded);
	CompressedChunkStorage(CompressedChunkStorage &&other) noexcept;
	CompressedChunkStorage(const CompressedChunkStorage &);
	CompressedChunkStorage &operator=(CompressedChunkStorage &&other) noexcept;
	CompressedChunkStorage &operator=(const CompressedChunkStorage &);
	~CompressedChunkStorage() = default;

	// Decompress into a plain 3D array
	void expand(ExpandedView expanded) const noexcept;

	// Set all values in the chunk to `value`
	void setUniform(bool value) noexcept;

	// True if all values in the chunk are equal
	bool uniform() const noexcept { return !m_nodes && (m_uniform_value_mask == 0 || ~m_uniform_value_mask == 0); }

	// Single element access. Behavior is undefined if
	// any of x, y or z is out of chunk boundaries.
	//
	// Access is not particularly fast, use `expand()`
	// if you plan to access many values at once.
	bool load(uint32_t x, uint32_t y, uint32_t z) const noexcept;
	// Same as `load(pos.x, pos.y, pos.z)
	bool operator[](glm::uvec3 pos) { return load(pos.x, pos.y, pos.z); }

private:
	struct Node {
		uint8_t m_leaf_mask[64];
	};

	uint64_t m_nonuniform_node_mask = 0;
	uint64_t m_uniform_value_mask = 0;
	std::unique_ptr<Node[]> m_nodes;
};

extern template class VOXEN_API CompressedChunkStorage<uint8_t>;
extern template class VOXEN_API CompressedChunkStorage<uint16_t>;
extern template class VOXEN_API CompressedChunkStorage<uint32_t>;

} // namespace voxen::land
