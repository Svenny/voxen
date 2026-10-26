#pragma once

#include <voxen/visibility.hpp>

#include <extras/pimpl.hpp>

namespace tk::cli
{

class Args;
class Parser;

} // namespace tk::cli

namespace voxen
{

namespace gfx
{

struct GfxOptions;

}

class VOXEN_API RuntimeConfig final {
public:
	RuntimeConfig();
	RuntimeConfig(RuntimeConfig &&) = delete;
	RuntimeConfig(const RuntimeConfig &) = delete;
	RuntimeConfig &operator=(RuntimeConfig &&) = delete;
	RuntimeConfig &operator=(const RuntimeConfig &) = delete;
	~RuntimeConfig() noexcept;

	// Options of the graphics subsystem
	const gfx::GfxOptions &gfxOptions() const noexcept;

	// Add commandline options related to each sub-config
	static void addOptions(tk::cli::Parser &parser);
	// Fill each sub-config from parsed commandline options
	void fill(const tk::cli::Args &args);

	// Get a global instance of this service
	static RuntimeConfig &instance() noexcept;

private:
	struct VOXEN_LOCAL Impl;
	extras::pimpl<Impl, 256, 8> m_impl;
};

} // namespace voxen
