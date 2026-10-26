#pragma once

#include <voxen/visibility.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace voxen
{

class VOXEN_API Config {
public:
	using Location = std::source_location;
	using option_t = std::variant<std::string, int64_t, double, bool>;

	struct SchemeEntry {
		std::string section;
		std::string parameter_name;
		std::string description;
		option_t default_value;
	};
	using Scheme = std::vector<SchemeEntry>;

	// Loads TOML config file at `relative_path` (relative to user data directory).
	// Options missing in the file, or having values of wrong types, get default values.
	// If the file does not exist, it is created with default values.
	// Existing files are never rewritten, to keep the user's formatting and comments.
	Config(std::filesystem::path relative_path, Scheme scheme);
	Config(Config&&) = delete;
	Config(const Config&) = delete;
	Config& operator=(Config&&) = delete;
	Config& operator=(const Config&) = delete;
	~Config() noexcept;

	// Changes option value for this run only, the file is not updated.
	// Throws voxen::Exception("wrong parameter value type"), voxen::Exception("Option not found"), voxen::Exception("Inconsistent types of option values")
	void patch(std::string_view section, std::string_view parameter_name, std::string_view value_string,
		Location loc = Location::current());

	std::optional<std::string> optionString(std::string_view section, std::string_view parameter_name) const;
	std::optional<int64_t> optionInt64(std::string_view section, std::string_view parameter_name) const;
	std::optional<int32_t> optionInt32(std::string_view section, std::string_view parameter_name) const;
	std::optional<double> optionDouble(std::string_view section, std::string_view parameter_name) const;
	std::optional<bool> optionBool(std::string_view section, std::string_view parameter_name) const;

	int32_t getInt32(std::string_view section, std::string_view parameter_name,
		Location loc = Location::current()) const;
	double getDouble(std::string_view section, std::string_view parameter_name,
		Location loc = Location::current()) const;
	bool getBool(std::string_view section, std::string_view parameter_name, Location loc = Location::current()) const;

public:
	// Global config scheme
	static Scheme mainConfigScheme();

	// Global config access point
	static Config* mainConfig();

	static std::string optionToString(option_t value);

	// TOML text written to a newly created config file, all options
	// have default values and descriptions as comments
	static std::string defaultFileText(const Scheme& scheme);

	// Throws std::invalid_argument, std::out_of_range
	static option_t optionFromString(std::string_view s, size_t type);

private:
	std::map<std::string, std::map<std::string, option_t, std::less<>>, std::less<>> m_data;
	std::filesystem::path m_path;

private:
	static std::unique_ptr<Config> g_instance;
	static const std::filesystem::path kMainConfigRelPath;
};

} // namespace voxen
