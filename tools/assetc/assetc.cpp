#include "font_atlas.hpp"

#include <tk/cli.hpp>

#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

namespace
{

constexpr char GROUP_FONT_ATLAS[] = "Font atlas (--type=font-atlas)";

tk::cli::Parser makeParser()
{
	using ValueType = tk::cli::Parser::ValueType;

	tk::cli::Parser parser("assetc",
		"Voxen asset compiler, converts source assets to game data files.\n"
		"Supported asset types: font-atlas.");

	parser.addFlag("help", 'h', "Display this help");
	parser.addFlag("verbose", 'v', "Print details of the compiled asset");
	parser.addOption({
		.name = "type",
		.short_name = 't',
		.type = ValueType::String,
		.description = "Asset type to compile",
		.default_value = std::nullopt,
		.group = {},
	});
	parser.addOption({
		.name = "input",
		.short_name = 'i',
		.type = ValueType::String,
		.description = "Source asset file",
		.default_value = std::nullopt,
		.group = {},
	});
	parser.addOption({
		.name = "output",
		.short_name = 'o',
		.type = ValueType::String,
		.description = "Compiled asset file, missing directories are created",
		.default_value = std::nullopt,
		.group = {},
	});

	parser.addOption({
		.name = "metrics",
		.short_name = 0,
		.type = ValueType::String,
		.description = "Write glyph metrics as C++ declarations to this file",
		.default_value = std::nullopt,
		.group = GROUP_FONT_ATLAS,
	});
	parser.addOption({
		.name = "glyph-size",
		.short_name = 0,
		.type = ValueType::Integer,
		.description = "Nominal glyph size in points",
		.default_value = "32",
		.group = GROUP_FONT_ATLAS,
	});
	parser.addOption({
		.name = "sdf-spread",
		.short_name = 0,
		.type = ValueType::Integer,
		.description = "SDF spread in pixels",
		.default_value = "4",
		.group = GROUP_FONT_ATLAS,
	});

	return parser;
}

int fail(std::string_view message)
{
	fprintf(stderr, "assetc: %.*s\n", int(message.size()), message.data());
	return 1;
}

} // namespace

int main(int argc, char *argv[])
{
	const tk::cli::Parser parser = makeParser();

	auto args = parser.parse(argc, argv);
	if (!args) {
		return fail(args.error());
	}

	if (args->flag("help")) {
		puts(parser.helpText().c_str());
		return 0;
	}

	if (!args->positional().empty()) {
		return fail("unexpected positional argument '" + args->positional().front() + "'");
	}

	auto type = args->value("type");
	auto input = args->value("input");
	auto output = args->value("output");
	if (!type || !input || !output) {
		return fail("'--type', '--input' and '--output' are required, see '--help'");
	}

	if (*type == "font-atlas") {
		const int64_t glyph_size = *args->integer("glyph-size");
		const int64_t sdf_spread = *args->integer("sdf-spread");
		// FreeType limits SDF spread to [2; 32]
		if (glyph_size < 1 || glyph_size > 1024 || sdf_spread < 2 || sdf_spread > 32) {
			return fail("glyph size or SDF spread is out of range");
		}

		auto result = assetc::compileFontAtlas({
			.font_path = *input,
			.atlas_path = *output,
			.metrics_path = args->value("metrics").value_or(std::string_view()),
			.glyph_size = int(glyph_size),
			.sdf_spread = int(sdf_spread),
			.verbose = args->flag("verbose"),
		});

		if (!result) {
			return fail(result.error());
		}

		return 0;
	}

	return fail("unknown asset type '" + std::string(*type) + "'");
}
