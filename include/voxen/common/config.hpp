#pragma once

#include <filesystem>
#include <variant>
#include <string>
#include <string_view>
#include <vector>
#include <map>

#define SI_CONVERT_GENERIC
#include <simpleini/SimpleIni.h>

namespace voxen
{

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
	~Config() noexcept;

	// Throws voxen::Exception("wrong parameter value type"), voxen::Exception("Option not found"), voxen::Exception("Inconsistent types of option values")
	void patch(std::string_view section, std::string_view parameter_name, option_t value, bool saveToConfigFile = false);

	// Throws std::bad_variant_access, voxen::Exception("Option not found")
	std::string optionString(std::string_view section, std::string_view parameter_name) const;
	int64_t optionInt64(std::string_view section, std::string_view parameter_name) const;
	int32_t optionInt32(std::string_view section, std::string_view parameter_name) const;
	double optionDouble(std::string_view section, std::string_view  parameter_name) const;
	bool optionBool(std::string_view section, std::string_view parameter_name) const;

	size_t optionType(std::string_view section, std::string_view parameter_name) const;

public:

	// Global config scheme
	static Scheme mainConfigScheme();

	// Global config access point
	static Config* mainConfig();

	static std::string optionToString(option_t value);

	// Throws std::invalid_argument, std::out_of_range
	static option_t optionFromString(std::string_view s, size_t type);

private:
	std::map<std::string, std::map<std::string, option_t, std::less<>>, std::less<>> m_data;
	std::filesystem::path m_path;
	CSimpleIniA m_ini;

private:
	static std::unique_ptr<Config> g_instance;
	static const std::filesystem::path kMainConfigRelPath;
};

}
