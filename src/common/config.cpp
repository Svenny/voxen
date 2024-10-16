#include <voxen/common/config.hpp>

#include <voxen/common/filemanager.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include <fmt/format.h>

#include <cassert>

using namespace std::filesystem;
using std::string_view;

namespace voxen
{

const path Config::kMainConfigRelPath = "configs/main.ini";
std::unique_ptr<Config> Config::g_instance = nullptr;

Config::Config(path path, Config::Scheme scheme) : m_path(path)
{
	m_ini.SetUnicode();
	std::string filepath = path.string();
	//NOTE(sirgienko) maybe we should check erros. Even if missing file is expected behaviour, other errors may happens
	m_ini.LoadFile(filepath.c_str());

	for (const SchemeEntry& entry : scheme) {
		option_t value;
		const char* value_ptr = m_ini.GetValue(entry.section.c_str(), entry.parameter_name.c_str());
		if (!value_ptr) {
			const std::string& value_str = optionToString(entry.default_value);
			// This logic work only for one-line description. For multiline, each new line should starts with '; '
			const std::string& comment = "; " + entry.description;
			m_ini.SetValue(entry.section.c_str(), entry.parameter_name.c_str(), value_str.c_str(), comment.c_str());
			value = entry.default_value;
		} else {
			size_t type = entry.default_value.index();
			value = Config::optionFromString(string_view(value_ptr), type);
		}

		if (m_data.count(entry.section) == 0) {
			m_data[entry.section] = std::map<std::string, option_t, std::less<>>();
		}
		m_data[entry.section][entry.parameter_name] = value;
	}
}

Config::~Config() noexcept
{
	// NOTE(sirgienko) Not sure, maybe we should move it in ::patch inside saveToConfigFile branch
	FileManager::makeDirsForFile(m_path);
	//TODO(sirgienko) checking save errors here from the method?
	m_ini.SaveFile(m_path.string().c_str());
}

Config* Config::mainConfig()
{
	if (!g_instance) {
		path config_path = FileManager::userDataPath() / kMainConfigRelPath;
		g_instance = std::make_unique<Config>(config_path, mainConfigScheme());
	}
	return g_instance.get();
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
	s.push_back({ "controller", "roll_speed", "Player roll speed", 1.5 * 0.01 });

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

void Config::patch(string_view section, string_view parameter_name, string_view value_string, bool saveToConfigFile,
	Location loc)
{
	if (auto it_ext = m_data.find(section); it_ext != m_data.end()) {
		if (auto it_inter = it_ext->second.find(parameter_name); it_inter != it_ext->second.end()) {
			it_inter->second = optionFromString(value_string, it_inter->second.index());

			if (saveToConfigFile) {
				const std::string& str = Config::optionToString(it_inter->second);
				m_ini.SetValue(section.data(), parameter_name.data(), str.c_str());
			}

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
		return (int64_t) std::stoi(s.data());

	case 2:
		static_assert(std::is_same_v<double, std::variant_alternative_t<2, Config::option_t>>);
		return std::stod(s.data());

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

} // namespace voxen
