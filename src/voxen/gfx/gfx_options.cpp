#include <voxen/gfx/gfx_options.hpp>

#include <tk/cli.hpp>

#include <optional>

namespace voxen::gfx
{

void GfxOptions::addCliOptions(tk::cli::Parser &parser)
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

void GfxOptions::fillFromCli(const tk::cli::Args &args)
{
	use_debugging = args.flag("gfx-debugging");
	use_validation = args.flag("gfx-validation");

	if (auto gpu_name = args.value("gfx-gpu-name"); gpu_name) {
		preferred_gpu_name = *gpu_name;
	}
}

} // namespace voxen::gfx
