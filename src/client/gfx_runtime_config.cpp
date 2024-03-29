#include <voxen/client/gfx_runtime_config.hpp>

#include <cxxopts/cxxopts.hpp>

namespace voxen::client
{

void GfxRuntimeConfig::addOptions(cxxopts::Options &opts)
{
	opts.add_options("Graphics")
		("gfx-debugging", "Enable graphics API debugging extensions")
		("gfx-validation", "Enable graphics API validation layers");
}

void GfxRuntimeConfig::fill(const cxxopts::ParseResult &result)
{
	m_use_debugging = result["gfx-debugging"].as<bool>();
	m_use_validation = result["gfx-validation"].as<bool>();
}

}
