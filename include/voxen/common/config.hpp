#pragma once

#include <filesystem>
#include <variant>
#include <string>
#include <string_view>
#include <vector>
#include <map>

#define SI_CONVERT_GENERIC
#include <simpleini/SimpleIni.h>

using std::string_view;

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
	~Config();

	// Throws voxen::Exception("wrong parameter value type"), voxen::Exception("Option not found"), voxen::Exception("Inconsistent types of option values")
	void patch(string_view section, string_view parameter_name, option_t value, bool saveToConfigFile = false);

	// Throws std::bad_variant_access, voxen::Exception("Option not found")
	std::string optionString(string_view section, string_view parameter_name) const;
	int64_t optionInt(string_view section, string_view parameter_name) const;
	double optionDouble(string_view section, string_view  parameter_name) const;
	bool optionBool(string_view section, string_view parameter_name) const;

	int optionType(string_view section, string_view parameter_name) const;

public:

	// Global config scheme
	static Scheme mainConfigScheme();

	// Global config access point
	static Config* mainConfig();

	static std::string optionToString(option_t value);

	// Trhows std::invalid_argument, std::out_of_range
	static option_t optionFromString(string_view s, int type);

private:
	std::map<std::string, std::map<std::string, option_t, std::less<>>, std::less<>> m_data;
	std::filesystem::path m_path;
	CSimpleIniA m_ini;

private:
	static std::unique_ptr<Config> g_instance;
	static const std::filesystem::path kMainConfigRelPath;
};

}
