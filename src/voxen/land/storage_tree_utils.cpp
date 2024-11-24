#include <voxen/land/storage_tree_utils.hpp>

#include "land_storage_tree_private.hpp"
#include "storage_tree_utils_private.hpp"

// BMI PDEP, BEXTR etc. intrinsics
#include <immintrin.h>

namespace voxen::land
{

static_assert(Consts::CHUNK_KEY_XZ_BITS < 32, ">31 ChunkKey XZ bits, StorageTree usage of int32 will break");
static_assert(Consts::CHUNK_KEY_Y_BITS < 32, ">31 ChunkKey Y bits, StorageTree usage of int32 will break");
static_assert(Consts::NUM_LOD_SCALES < (1u << Consts::CHUNK_KEY_Y_BITS), "Not enough ChunkKey scale bits");

static_assert(1u << (Consts::NUM_LOD_SCALES - 1) == detail::DuoctreeLargestNode::NODE_SIZE_CHUNKS,
	"More LOD scales than StorageTree can store");
static_assert(Consts::NUM_LOD_SCALES == detail::DuoctreeLargestNode::NODE_SCALE_LOG2 + 1,
	"More LOD scales than StorageTree can store");

// Can't fail as bounds are calculated from `NUM_LOD_SCALES` but just in case
static_assert(Consts::MIN_WORLD_Y_CHUNK == -detail::DuoctreeLargestNode::NODE_SIZE_CHUNKS);
static_assert(Consts::MAX_WORLD_Y_CHUNK == detail::DuoctreeLargestNode::NODE_SIZE_CHUNKS - 1);

// These masks define where to deposit/gather X/Y/Z bits using the PDEP/PEXT BMI2 instructions.
// Inside one node, child index bits are laid out in Morton order.
//
// `Root` is filled manually as it needs special wraparound handling.
// Root items are laid out in XZ order without Morton ordering.
//
// Some of the parts from `x1` to `x256` gets the highest bit (7) set as stop bit.
// That same part can also get the second-high bit (6) set as subnode bit - then it means
// the access targets odd-scale key
//
// `x1` part is not useful for traverse, so its lower 3 bits store "subnode selector"
// which is just a YXZ child index for this node if we were in an octree, not duoctree.
// This selector is useful to store odd-LOD data in duoctree nodes, we track
// odd-scale key insertion based on it in combination with subnode bit.
//
// This decomposition is invertible up to wraparound - the original key or its
// wrapped (in-bounds) equivalent can be precisely restored from these bits.
//
//    ~ unused bits
//    # root index bits, how many used depends on root items number
//    S stop bit (stop descending at this duoctree node)
//    N subnode bit (set when targeting odd LOD scale)
//    X/Y/Z child node indexing bits
//
//                                    Root   TriQRoot  Bridge    x256      x64      x16      x4       x1
//                                  ######## ~~XZXZXZ ~YXZXZXZ SNYXZYXZ SNYXZYXZ SNYXZYXZ SNYXZYXZ S~~~~XYZ
constexpr static uint64_t XMASK = 0b00000000'00101010'00101010'00010010'00010010'00010010'00010010'00000000;
constexpr static uint64_t YMASK = 0b00000000'00000000'01000000'00100100'00100100'00100100'00100100'00000000;
constexpr static uint64_t ZMASK = 0b00000000'00010101'00010101'00001001'00001001'00001001'00001001'00000000;

std::optional<uint64_t> StorageTreeUtils::keyToTreePath(ChunkKey key) noexcept
{
	// Unpack bitfields
	int32_t x = key.x;
	int32_t y = key.y;
	int32_t z = key.z;
	uint32_t scale_log2 = key.scale_log2;

	if (y < Consts::MIN_WORLD_Y_CHUNK || y > Consts::MAX_WORLD_Y_CHUNK) [[unlikely]] {
		// Out of world height bounds
		return std::nullopt;
	}

	if (scale_log2 > detail::DuoctreeLargestNode::NODE_SCALE_LOG2) [[unlikely]] {
		// Too large scale, out of duoctree aggregation levels
		return std::nullopt;
	}

	if ((x | y | z) & ((1 << scale_log2) - 1)) [[unlikely]] {
		// At least one dimension is not aligned to the power of two grid
		return std::nullopt;
	}

	// Wrap X/Z coordinates around to create torus topology.
	//
	// Additional `WORLD_* / 2` is added to move origin - our XZ chunk plane
	// is within (-N; -M) - (N - 1; M - 1) range without chunk wrapping.
	// However, this is more of a formality to make math more consistent.
	constexpr uint32_t RSZ = Consts::STORAGE_TREE_ROOT_ITEM_SIZE_CHUNKS;
	constexpr int32_t WORLD_X = RSZ * Consts::STORAGE_TREE_ROOT_ITEMS_X;
	constexpr int32_t WORLD_Z = RSZ * Consts::STORAGE_TREE_ROOT_ITEMS_Z;

	int32_t x_wrapped = (x + WORLD_X / 2) % WORLD_X;
	int32_t z_wrapped = (z + WORLD_Z / 2) % WORLD_Z;

	uint32_t x_root = static_cast<uint32_t>(x_wrapped < 0 ? x_wrapped + WORLD_X : x_wrapped) / RSZ;
	uint32_t z_root = static_cast<uint32_t>(z_wrapped < 0 ? z_wrapped + WORLD_Z : z_wrapped) / RSZ;
	uint64_t root_selector = static_cast<uint64_t>(x_root * Consts::STORAGE_TREE_ROOT_ITEMS_Z + z_root) << (64 - 8);

	// Place stop bit in the byte corresponding to the duoctree/chunk level we should stop at.
	// Place subnode bit in the same place, just one bit below.
	// Shift right L/2 bytes + 6 bits to occupy bits 6 and 7.
	uint32_t stop_subnode_bit_shift_pos = 8 * ((scale_log2 + 1) / 2) + 6;
	// 2 - stop bit, always present, 1 - subnode bit, only for odd LODs
	uint64_t stop_subnode_bits = uint64_t(2 | (scale_log2 & 1)) << stop_subnode_bit_shift_pos;

	// Collect bits at `scale_log2` position to form subnode selector
	uint32_t snx = _bextr_u32(static_cast<uint32_t>(x), scale_log2, 1);
	uint32_t sny = _bextr_u32(static_cast<uint32_t>(y), scale_log2, 1);
	uint32_t snz = _bextr_u32(static_cast<uint32_t>(z), scale_log2, 1);
	uint64_t subnode_selector = (sny << 2) | (snx << 1) | snz;

	return subnode_selector | stop_subnode_bits | root_selector | _pdep_u64(static_cast<uint64_t>(x), XMASK)
		| _pdep_u64(static_cast<uint64_t>(y), YMASK) | _pdep_u64(static_cast<uint64_t>(z), ZMASK);
}

ChunkKey StorageTreeUtils::treePathToKey(uint64_t tree_path) noexcept
{
	// Reverse PDEP instruction effect - gather X/Y/Z bits without the root index part.
	// They mean offset from the minimal coordinate corner of the corresponding root item.
	uint64_t x = _pext_u64(tree_path, XMASK);
	uint64_t y = _pext_u64(tree_path, YMASK);
	uint64_t z = _pext_u64(tree_path, ZMASK);

	auto bytes = std::bit_cast<std::array<uint8_t, 8>>(tree_path);
	(void) bytes;

	// This is the bit stored in "Y negative" bit of the bridge node, see `YMASK`.
	constexpr uint64_t y_sign_bit = uint64_t(1) << 8;
	constexpr uint64_t y_sign_fill_mask = ~((y_sign_bit << 1) - 1);

	if (y & y_sign_bit) {
		// Fill in lost bits to make it a correct two's complement negative value
		y |= y_sign_fill_mask;
	}

	// Find root item index, it's the only thing encoded in the highest byte
	uint32_t root_component = tree_path >> (64 - 8);
	glm::ivec3 coord = calcRootItemMinCoord(root_component);

	coord.x += static_cast<int32_t>(x);
	coord.y = static_cast<int32_t>(y); // Note: not adding, overwriting
	coord.z += static_cast<int32_t>(z);

	// Now find the LOD scale by brute force. Step by two by duoctree levels
	for (uint32_t lod = 2; lod < Consts::NUM_LOD_SCALES; lod += 2) {
		uint64_t test_stop_bit = uint64_t(128) << (8 * lod / 2);
		uint64_t test_subnode_bit = uint64_t(64) << (8 * lod / 2);

		if (tree_path & test_stop_bit) {
			if (tree_path & test_subnode_bit) {
				// This was indexing subnode of odd LOD, one smaller than the duoctree LOD
				lod--;
			}

			return ChunkKey(coord, lod);
		}
	}

	// This path goes all the way down to a chunk
	return ChunkKey(coord);
}

} // namespace voxen::land
