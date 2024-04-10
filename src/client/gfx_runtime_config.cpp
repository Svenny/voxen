#include <voxen/client/gfx_runtime_config.hpp>

#include <cxxopts/cxxopts.hpp>

namespace voxen::client
{

void GfxRuntimeConfig::addOptions(cxxopts::Options &opts)
{
	// clang-format off: breaks nice chaining syntax
	opts.add_options("Graphics")
		("gfx-debugging", "Enable graphics API debugging extensions")
		("gfx-validation", "Enable graphics API validation layers");
	// clang-format on
}

void GfxRuntimeConfig::fill(const cxxopts::ParseResult &result)
{
	m_use_debugging = result["gfx-debugging"].as<bool>();
	m_use_validation = result["gfx-validation"].as<bool>();
}

} // namespace voxen::client
