#include "../voxen_test_common.hpp"

#include <voxen/version.hpp>

#include <fmt/format.h>

namespace voxen::test
{

svc::Engine::Ptr createEngine()
{
	// Test suite shares repository with the engine,
	// hence it has the same versioning and commit hash
	return svc::Engine::create(svc::EngineStartArgs(svc::AppInfo {
		.name = "Voxen Test Suite",
		.version_major = Version::MAJOR,
		.version_minor = Version::MINOR,
		.version_patch = Version::PATCH,
		.version_appendix = Version::SUFFIX,
		.git_commit_hash = Version::GIT_HASH,
	}));
}

} // namespace voxen::test

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
