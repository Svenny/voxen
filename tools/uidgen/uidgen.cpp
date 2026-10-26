#include <tk/cli.hpp>
#include <tk/uid.hpp>

#include <cstdint>
#include <cstdio>

int main(int argc, char *argv[])
{
	tk::cli::Parser parser("uidgen", "Generates random UIDs in `tk::UID` string format");
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
		char chars[tk::UID::CHAR_REPR_LENGTH];
		tk::UID::generateRandom().toChars(chars);
		puts(chars);
	}

	return 0;
}
