#include <voxen/svc/asset_decoder.hpp>

#include <voxen/common/pipe_memory_allocator.hpp>

#include <coroutine>

namespace voxen::svc
{

void IAssetDecoder::Deleter::operator()(IAssetDecoder* ptr) noexcept
{
	ptr->~IAssetDecoder();
	PipeMemoryAllocator::deallocate(ptr);
}

IAssetDecoder::~IAssetDecoder() = default;

} // namespace voxen::svc
