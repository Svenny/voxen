#include "../voxen_test_common.hpp"

#include <fmt/format.h>

namespace Catch
{

std::string StringMaker<voxen::land::ChunkKey>::convert(voxen::land::ChunkKey key)
{
	auto x = key.x;
	auto y = key.y;
	auto z = key.z;
	return fmt::format("({}, {}, {} | L{})", x, y, z, key.scaleLog2());
}

} // namespace Catch
