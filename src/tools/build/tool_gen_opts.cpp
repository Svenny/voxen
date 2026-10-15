#include <tk/cli.hpp>
#include <tk/json.hpp>

#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char *argv[])
{
	tk::cli::Parser parser("gen-opts", "Codegen tool for options tables");
	parser.addFlag("help", 'h', "Display this help");

	auto args = parser.parse(argc, argv);
	if (!args) {
		std::cerr << args.error() << std::endl;
		return 1;
	}

	if (args->flag("help") || args->positional().empty()) {
		std::cout << parser.helpText() << std::endl;
		return 0;
	}

	const std::string &path = args->positional().front();
	std::ifstream f(path);
	std::stringstream ss;
	ss << f.rdbuf();

	auto val = tk::json::parse(ss.str(), tk::json::Dialect::Json5);
	if (!val) {
		std::cerr << path << ":" << val.error().toString() << std::endl;
		return 1;
	}

	const tk::json::Object *table = val->get<tk::json::Object>();
	const tk::json::Value *options = table ? table->find("options") : nullptr;
	const tk::json::Array *options_array = options ? options->get<tk::json::Array>() : nullptr;
	std::cout << "Parsed " << (options_array ? options_array->size() : 0) << " options" << std::endl;

	// Steps:
	// - Read JSON5 declaring an options table
	// - Options tables are independent (for now?), no recursive pull
	// - Validate, report errors
	// - Generate table declaration header (public part)
	// - Generate hidden part (table read/write/validation code)

	return 0;
}
