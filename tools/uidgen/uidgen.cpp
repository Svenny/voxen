#include <cinttypes>
#include <cstdio>

import tk.cli;
import tk.rng;

int main(int argc, char *argv[])
{
	tk::cli::Parser parser("uidgen", "Generates random UIDs in `voxen::UID` string format");
	parser.addFlag("help", 'h', "Display this help");
	parser.addOption({
		.name = "count",
		.short_name = 'n',
		.type = tk::cli::Parser::ValueType::Integer,
		.description = "Number of UIDs to generate",
		.default_value = "1",
		.group = {},
	});

	auto args = parser.parse(argc, argv);
	if (!args) {
		fprintf(stderr, "%s\n", args.error().c_str());
		return 1;
	}

	if (args->flag("help")) {
		puts(parser.helpText().c_str());
		return 0;
	}

	const int64_t count = args->integer("count").value_or(1);
	if (count < 1 || !args->positional().empty()) {
		puts(parser.helpText().c_str());
		return 1;
	}

	for (int64_t i = 0; i < count; i++) {
		const uint64_t v0 = tk::rng::entropySeed();
		const uint64_t v1 = tk::rng::entropySeed();

		// Same format as `voxen::UID::toChars()`
		printf("%08" PRIx32 "-%08" PRIx32 "-%08" PRIx32 "-%08" PRIx32 "\n", uint32_t(v0 >> 32u), uint32_t(v0),
			uint32_t(v1 >> 32u), uint32_t(v1));
	}

	return 0;
}
