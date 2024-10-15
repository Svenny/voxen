#include <voxen/common/land/block.hpp>

namespace voxen::land
{

// Define in .cpp to suppress -Wweak-vtables
IBlock::~IBlock() noexcept = default;

} // namespace voxen::land
