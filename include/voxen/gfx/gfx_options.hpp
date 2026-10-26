#pragma once

#include <string>

namespace tk::cli
{

class Args;
class Parser;

} // namespace tk::cli

namespace voxen::gfx
{

// Options of the graphics subsystem.
// TODO: generate from `gfx_options.json5` once options codegen is implemented.
struct GfxOptions {
	// Whether to enable graphics API debugging extensions
	// (debug callbacks, object naming, cmdstream labels etc.)
	bool use_debugging = false;
	// Whether to enable validation layers for graphics API
	bool use_validation = false;
	// Name of the preferred GPU (as exposed by graphics API driver).
	// If the name is empty or is not available then GPU will be auto-selected.
	std::string preferred_gpu_name;

	// Add commandline options related to these options
	static void addCliOptions(tk::cli::Parser &parser);
	// Fill options from parsed commandline options
	void fillFromCli(const tk::cli::Args &args);
};

} // namespace voxen::gfx
