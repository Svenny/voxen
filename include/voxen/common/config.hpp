#pragma once

#include <filesystem>
#include <variant>
#include <string>
#include <vector>

namespace voxen {

class Config {
public:
	using option_t = std::variant<std::string, int64_t, double, bool>;
	struct SchemeEntry {
		std::string section;
		std::string parameter_name;
		std::string description;
		option_t default_value;
	};
	using Scheme = std::vector<SchemeEntry>;

	Config(std::filesystem::path config_filepath, Scheme scheme);

	// Throws voxen::Exception("wrong parameter value type"), voxen::Exception("Option not found"), voxen::Exception("Inconsistent types of option values")
	void patch(std::string section, std::string parameter_name, option_t value);

	//void save(); //? Not always save changes in settings?

	// Throws std::bad_variant_access, voxen::Exception("Option not found")
	std::string optionString(std::string section, std::string parameter_name);
	int64_t optionInt(std::string section, std::string parameter_name);
	double optionDouble(std::string section, std::string parameter_name);
	bool optionBool(std::string section, std::string parameter_name);

	int optionType(std::string section, std::string parameter_name);

	/// Global config scheme
	static Scheme mainConfigScheme();

	/// Global config access point
	static Config* mainConfig();

private:
	Scheme debug_stored_scheme; //TODO DEV and temporaraly!

private:
	static Config* g_instance;
	static const std::filesystem::path kMainConfigRelPath;
};

}
