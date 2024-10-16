#pragma once

#include <voxen/visibility.hpp>

#include <extras/pimpl.hpp>

namespace cxxopts
{

class Options;
class ParseResult;

} // namespace cxxopts

namespace voxen
{

namespace client
{

class GfxRuntimeConfig;

}

class VOXEN_API RuntimeConfig final {
public:
	RuntimeConfig();
	RuntimeConfig(RuntimeConfig &&) = delete;
	RuntimeConfig(const RuntimeConfig &) = delete;
	RuntimeConfig &operator=(RuntimeConfig &&) = delete;
	RuntimeConfig &operator=(const RuntimeConfig &) = delete;
	~RuntimeConfig() noexcept;

	// Sub-config for graphics subsystem
	const client::GfxRuntimeConfig &gfxConfig() const noexcept;

	// Add commandline options related to each sub-config
	static void addOptions(cxxopts::Options &opts);
	// Fill each sub-config from parsed commandline options
	void fill(const cxxopts::ParseResult &result);

	// Get a global instance of this service
	static RuntimeConfig &instance() noexcept;

private:
	struct VOXEN_LOCAL Impl;
	extras::pimpl<Impl, 256, 8> m_impl;
};

} // namespace voxen
