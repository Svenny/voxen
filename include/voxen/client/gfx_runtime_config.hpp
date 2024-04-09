#pragma once

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

	// Add commandline options related to this config
	static void addOptions(cxxopts::Options &opts);
	// Fill config from parsed commandline options
	void fill(const cxxopts::ParseResult &result);

private:
	bool m_use_debugging = false;
	bool m_use_validation = false;
};

} // namespace voxen::client
