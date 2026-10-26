#include <voxen/assets/asset_decoder.hpp>

#include <voxen/common/pipe_memory_allocator.hpp>

namespace voxen::assets
{

void IAssetDecoder::Deleter::operator()(IAssetDecoder* ptr) noexcept
{
	ptr->~IAssetDecoder();
	PipeMemoryAllocator::deallocate(ptr);
}

IAssetDecoder::~IAssetDecoder() = default;

} // namespace voxen::assets
