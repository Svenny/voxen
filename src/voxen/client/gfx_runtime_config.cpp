#include <voxen/client/gfx_runtime_config.hpp>

#include <optional>

import tk.cli;

namespace voxen::client
{

void GfxRuntimeConfig::addOptions(tk::cli::Parser &parser)
{
	parser.addFlag("gfx-debugging", 0, "Enable graphics API debugging extensions", "Graphics");
	parser.addFlag("gfx-validation", 0, "Enable graphics API validation layers", "Graphics");
	parser.addOption({
		.name = "gfx-gpu-name",
		.short_name = 0,
		.type = tk::cli::Parser::ValueType::String,
		.description = "Name of GPU to use (empty to auto-select)",
		.default_value = std::nullopt,
		.group = "Graphics",
	});
}

void GfxRuntimeConfig::fill(const tk::cli::Args &args)
{
	m_use_debugging = args.flag("gfx-debugging");
	m_use_validation = args.flag("gfx-validation");

	if (auto gpu_name = args.value("gfx-gpu-name"); gpu_name) {
		m_preferred_gpu_name = *gpu_name;
	}
}

} // namespace voxen::client
