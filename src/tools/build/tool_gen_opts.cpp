#include <cxxopts/cxxopts.hpp>

#include <json5/json5cpp.h>

#include <fstream>
#include <iostream>

int main(int argc, char *argv[])
{
	cxxopts::Options options("gen-opts", "Codegen tool for options tables");
	// clang-format off: breaks nice chaining syntax
	options.add_options()
		("h,help", "Display this help");
	// clang-format on

	cxxopts::ParseResult parse_result = options.parse(argc, argv);
	if (argc == 1 || parse_result.count("help")) {
		std::cout << options.help() << std::endl;
		return 0;
	}

	std::ifstream f(argv[1]);
	Json::Value val;
	if (!Json5::parse(f, val)) {
		std::cout << "error!" << std::endl;
		return 1;
	}

	std::cout << val << std::endl;

	// Steps:
	// - Read JSON5 declaring an options table
	// - Options tables are independent (for now?), no recursive pull
	// - Validate, report errors
	// - Generate table declaration header (public part)
	// - Generate hidden part (table read/write/validation code)

	return 0;
}
