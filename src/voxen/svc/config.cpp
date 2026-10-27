#include <voxen/svc/config.hpp>

#include <voxen/error_condition.hpp>
#include <voxen/exception.hpp>
#include <voxen/log.hpp>
#include <voxen/svc/file_manager.hpp>

#include <tk/toml.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <charconv>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <utility>

using namespace std::filesystem;
using std::string_view;

namespace voxen::svc
{

const path Config::MAIN_CONFIG_REL_PATH = "configs/main.toml";
std::unique_ptr<Config> Config::s_instance = nullptr;

namespace
{

std::optional<Config::option_t> optionFromToml(const tk::toml::Value &value, size_t type)
{
	switch (type) {
	case 0:
		static_assert(std::is_same_v<std::string, std::variant_alternative_t<0, Config::option_t>>);
		if (const std::string *str = value.get<std::string>(); str) {
			return *str;
		}
		break;

	case 1:
		static_assert(std::is_same_v<int64_t, std::variant_alternative_t<1, Config::option_t>>);
		if (const int64_t *integer = value.get<int64_t>(); integer) {
			return *integer;
		}
		break;

	case 2:
		static_assert(std::is_same_v<double, std::variant_alternative_t<2, Config::option_t>>);
		if (const double *number = value.get<double>(); number) {
			return *number;
		}
		// Allow writing integer values without `.0`
		if (const int64_t *integer = value.get<int64_t>(); integer) {
			return double(*integer);
		}
		break;

	case 3:
		static_assert(std::is_same_v<bool, std::variant_alternative_t<3, Config::option_t>>);
		if (const bool *boolean = value.get<bool>(); boolean) {
			return *boolean;
		}
		break;

	default:
		static_assert(std::variant_size_v<Config::option_t> == 4);
		break;
	}

	return std::nullopt;
}

// Unlike `std::stoi`/`std::stod`, requires the whole string to be a number
// and covers the full `int64_t` range. Throws like them on failure.
template<typename T>
T parseNumber(std::string_view s)
{
	T value = 0;
	auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);

	if (ec == std::errc::result_out_of_range) {
		throw std::out_of_range("config option value is out of range");
	}

	if (ec != std::errc() || ptr != s.data() + s.size()) {
		throw std::invalid_argument("config option value is not a number");
	}

	return value;
}

tk::toml::Value optionToToml(const Config::option_t &value)
{
	return std::visit([](const auto &v) { return tk::toml::Value(v); }, value);
}

} // namespace

std::string Config::defaultFileText(const Scheme &scheme)
{
	std::vector<std::string_view> sections;
	for (const Config::SchemeEntry &entry : scheme) {
		if (std::find(sections.begin(), sections.end(), entry.section) == sections.end()) {
			sections.push_back(entry.section);
		}
	}

	std::string text = "# Generated with default values. Remove an option to reset it to the default.\n";

	for (std::string_view section : sections) {
		text += fmt::format("\n[{}]\n", tk::toml::formatKey(section));

		for (const Config::SchemeEntry &entry : scheme) {
			if (entry.section != section) {
				continue;
			}

			// Keep multi-line descriptions inside the comment
			std::string comment;
			for (char c : entry.description) {
				comment += c;
				if (c == '\n') {
					comment += "# ";
				}
			}

			text += fmt::format("# {}\n{} = {}\n", comment, tk::toml::formatKey(entry.parameter_name),
				tk::toml::formatValue(optionToToml(entry.default_value)));
		}
	}

	return text;
}

Config::Config(path relative_path, Config::Scheme scheme) : m_path(std::move(relative_path))
{
	std::optional<tk::toml::Table> file_data;

	std::error_code ec;
	if (std::filesystem::exists(FileManager::userDataPath() / m_path, ec)) {
		if (std::optional<std::string> text = FileManager::readUserTextFile(m_path); text) {
			if (auto result = tk::toml::parse(*text); result) {
				file_data = std::move(*result);
			} else {
				Log::error("Can't parse config file '{}', using default values: {}", m_path.string(),
					result.error().toString());
			}
		} else {
			Log::error("Can't read config file '{}', using default values", m_path.string());
		}
	} else {
		Log::info("Config file '{}' does not exist, creating it with default values", m_path.string());
		if (!FileManager::writeUserTextFile(m_path, defaultFileText(scheme))) {
			Log::warn("Can't write config file '{}'", m_path.string());
		}
	}

	for (const SchemeEntry &entry : scheme) {
		option_t value = entry.default_value;

		const tk::toml::Value *section = file_data ? file_data->find(entry.section) : nullptr;
		const tk::toml::Table *section_table = section ? section->get<tk::toml::Table>() : nullptr;
		const tk::toml::Value *file_value = section_table ? section_table->find(entry.parameter_name) : nullptr;

		if (file_value) {
			if (auto converted = optionFromToml(*file_value, entry.default_value.index()); converted) {
				value = std::move(*converted);
			} else {
				Log::warn("Config option {}.{} in '{}' has wrong type, using the default value", entry.section,
					entry.parameter_name, m_path.string());
			}
		}

		m_data[entry.section][entry.parameter_name] = std::move(value);
	}
}

Config::~Config() noexcept = default;

Config *Config::mainConfig()
{
	if (!s_instance) {
		s_instance = std::make_unique<Config>(MAIN_CONFIG_REL_PATH, mainConfigScheme());
	}
	return s_instance.get();
}

Config::Scheme Config::mainConfigScheme()
{
	Config::Scheme s;

	s.push_back({ "dev", "fps_logging", "Enable FPS and UPS logging", false });
	s.push_back({ "window", "width", "Voxen window width", 1600L });
	s.push_back({ "window", "height", "Voxen window height", 900L });
	s.push_back({ "window", "fullscreen", "Enable fullscreen for Voxen window", false });
	s.push_back({ "controller", "mouse_sensitivity", "Mouse sensitivity", 1.5 });
	s.push_back({ "controller", "forward_speed", "Player forward speed", 100.0 });
	s.push_back({ "controller", "strafe_speed", "Player strafe speed", 50.0 });
	s.push_back({ "controller", "roll_speed", "Player roll speed", 1.5 });

	return s;
}

std::optional<bool> Config::optionBool(string_view section, string_view parameter_name) const
{
	auto it_ext = m_data.find(section);
	if (it_ext != m_data.end()) {
		auto it_inter = it_ext->second.find(parameter_name);
		if (it_inter != it_ext->second.end()) {
			return std::get<bool>(it_inter->second);
		}
	}

	return std::nullopt;
}

std::optional<double> Config::optionDouble(string_view section, string_view parameter_name) const
{
	auto it_ext = m_data.find(section);
	if (it_ext != m_data.end()) {
		auto it_inter = it_ext->second.find(parameter_name);
		if (it_inter != it_ext->second.end()) {
			return std::get<double>(it_inter->second);
		}
	}

	return std::nullopt;
}

std::optional<int64_t> Config::optionInt64(string_view section, string_view parameter_name) const
{
	auto it_ext = m_data.find(section);
	if (it_ext != m_data.end()) {
		auto it_inter = it_ext->second.find(parameter_name);
		if (it_inter != it_ext->second.end()) {
			return std::get<int64_t>(it_inter->second);
		}
	}

	return std::nullopt;
}

std::optional<int32_t> Config::optionInt32(string_view section, string_view parameter_name) const
{
	std::optional<int64_t> opt = optionInt64(section, parameter_name);
	if (!opt.has_value()) {
		return std::nullopt;
	}

	int64_t value = *opt;
	assert(value >= INT32_MIN && value <= INT32_MAX);
	return static_cast<int32_t>(value);
}

std::optional<std::string> Config::optionString(string_view section, string_view parameter_name) const
{
	auto it_ext = m_data.find(section);
	if (it_ext != m_data.end()) {
		auto it_inter = it_ext->second.find(parameter_name);
		if (it_inter != it_ext->second.end()) {
			return std::get<std::string>(it_inter->second);
		}
	}

	return std::nullopt;
}

int32_t Config::getInt32(string_view section, string_view parameter_name, Location loc) const
{
	if (auto opt = optionInt32(section, parameter_name); opt.has_value()) {
		return *opt;
	}

	Log::error("Option {}/{} (int32) not found", section, parameter_name);
	throw Exception::fromError(VoxenErrc::OptionMissing, "missing config option assumed existing", loc);
}

double Config::getDouble(string_view section, string_view parameter_name, Location loc) const
{
	if (auto opt = optionDouble(section, parameter_name); opt.has_value()) {
		return *opt;
	}

	Log::error("Option {}/{} (double) not found", section, parameter_name);
	throw Exception::fromError(VoxenErrc::OptionMissing, "missing config option assumed existing", loc);
}

bool Config::getBool(string_view section, string_view parameter_name, Location loc) const
{
	if (auto opt = optionBool(section, parameter_name); opt.has_value()) {
		return *opt;
	}

	Log::error("Option {}/{} (bool) not found", section, parameter_name);
	throw Exception::fromError(VoxenErrc::OptionMissing, "missing config option assumed existing", loc);
}

void Config::patch(string_view section, string_view parameter_name, string_view value_string, Location loc)
{
	if (auto it_ext = m_data.find(section); it_ext != m_data.end()) {
		if (auto it_inter = it_ext->second.find(parameter_name); it_inter != it_ext->second.end()) {
			it_inter->second = optionFromString(value_string, it_inter->second.index());
			return;
		}
	}

	Log::error("Option {}/{} not found for patching", section, parameter_name);
	throw Exception::fromError(VoxenErrc::OptionMissing, "missing config option for patching", loc);
}

std::string Config::optionToString(option_t value)
{
	using namespace std;

	size_t type_idx = value.index();
	switch (type_idx) {
	case 0:
		static_assert(is_same_v<string, variant_alternative_t<0, Config::option_t>>);
		return get<string>(value);

	case 1:
		static_assert(is_same_v<int64_t, variant_alternative_t<1, Config::option_t>>);
		return to_string(get<int64_t>(value));

	case 2:
		static_assert(is_same_v<double, variant_alternative_t<2, Config::option_t>>);
		return to_string(get<double>(value));

	case 3:
		static_assert(is_same_v<bool, variant_alternative_t<3, Config::option_t>>);
		return get<bool>(value) ? "true" : "false";

	default:
		static_assert(std::variant_size_v<Config::option_t> == 4);
		return "";
	}
}

Config::option_t Config::optionFromString(string_view s, size_t type)
{
	switch (type) {
	case 0:
		static_assert(std::is_same_v<std::string, std::variant_alternative_t<0, Config::option_t>>);
		return std::string(s);

	case 1:
		static_assert(std::is_same_v<int64_t, std::variant_alternative_t<1, Config::option_t>>);
		return parseNumber<int64_t>(s);

	case 2:
		static_assert(std::is_same_v<double, std::variant_alternative_t<2, Config::option_t>>);
		return parseNumber<double>(s);

	case 3: {
		static_assert(std::is_same_v<bool, std::variant_alternative_t<3, Config::option_t>>);
		if (s.size() != 4) {
			return false;
		}
		bool isTrueStr = tolower(s[0]) == 't';
		isTrueStr &= tolower(s[1]) == 'r';
		isTrueStr &= tolower(s[2]) == 'u';
		isTrueStr &= tolower(s[3]) == 'e';
		return isTrueStr;
	}

	default:
		static_assert(std::variant_size_v<Config::option_t> == 4);
		return "";
	}
}

} // namespace voxen::svc
