#pragma once

#include <string>

namespace cxxopts
{

class Options;
class ParseResult;

} // namespace cxxopts

namespace voxen::client
{

class GfxRuntimeConfig final {
public:
	// Whether to enable graphics API debugging extensions
	// (debug callbacks, object naming, cmdstream labels etc.).
	bool useDebugging() const noexcept { return m_use_debugging; }
	// Whether to enable validation layers for graphics API
	bool useValidation() const noexcept { return m_use_validation; }
	// Name of the preferred GPU (as exposed by graphics API driver).
	// If the name is empty or is not available then GPU will be auto-selected.
	const std::string &preferredGpuName() const noexcept { return m_preferred_gpu_name; }

	// Add commandline options related to this config
	static void addOptions(cxxopts::Options &opts);
	// Fill config from parsed commandline options
	void fill(const cxxopts::ParseResult &result);

private:
	bool m_use_debugging = false;
	bool m_use_validation = false;
	std::string m_preferred_gpu_name;
};

} // namespace voxen::client
