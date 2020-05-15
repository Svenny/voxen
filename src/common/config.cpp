#include <voxen/common/config.hpp>

#include <voxen/util/exception.hpp>
#include <voxen/util/voxen_home.hpp>
#include <voxen/util/log.hpp>

#include <fmt/format.h>

using namespace voxen;
using namespace std::filesystem;

const path Config::kMainConfigRelPath = "config.ini";
std::unique_ptr<Config> Config::g_instance = nullptr;

Config::Config(path path, Config::Scheme scheme): m_path(path) {
	m_ini.SetUnicode();
	std::string filepath = path.string();
	m_ini.LoadFile(filepath.c_str());

	for (const SchemeEntry& entry : scheme) {
		option_t value;
		const char* value_ptr = m_ini.GetValue(entry.section.c_str(), entry.parameter_name.c_str());
		if (value_ptr == NULL) {
			const std::string& value_str = optionToString(entry.default_value);
			// This logic work only for one-line description. For multiline, each new line should starts with '; '
			const std::string& comment = "; " + entry.description;
			m_ini.SetValue(entry.section.c_str(), entry.parameter_name.c_str(), value_str.c_str(), comment.c_str());
			value = entry.default_value;
		} else {
			int type = entry.default_value.index();
			value = Config::optionFromString(std::string_view(value_ptr), type);
		}
		if (m_data.count(entry.section) == 0)
			m_data[entry.section] = std::map<std::string, option_t, std::less<>>();
		m_data[entry.section][entry.parameter_name] = value;
	}
}
voxen::Config::~Config()
{
	// Not sure, maybe we should move it in ::patch inside saveToConfigFile branch
	m_ini.SaveFile(m_path.string().c_str());
}

Config* Config::mainConfig() {
	if(!g_instance) {
		path config_path = voxenHome() / kMainConfigRelPath;
		g_instance = std::make_unique<Config>(config_path, mainConfigScheme());
	}
	return g_instance.get();
}

Config::Scheme voxen::Config::mainConfigScheme()
{
	Config::Scheme s;

	s.push_back({"dev", "fps_logging", "This parameter enable fps and ups logging into log", false});
	s.push_back({"window", "width", "This parameter control voxen window width", 1600L});
	s.push_back({"window", "height", "This parameter control voxen window height", 900L});
	s.push_back({"window", "fullscreen", "This parameter enable fullscreen for voxen window", false});

	return s;
}

bool voxen::Config::optionBool(string_view section, string_view parameter_name) const
{
	auto it_ext = m_data.find(section);
	if (it_ext != m_data.end()) {
		auto it_inter = it_ext->second.find(parameter_name);
		if (it_inter != it_ext->second.end())
			return std::get<bool>(it_inter->second);
	}

	throw voxen::FormattedMessageException("Bool option {}/{} not found", fmt::make_format_args(section, parameter_name));
	return false;
}

double voxen::Config::optionDouble(string_view section, string_view parameter_name) const
{
	auto it_ext = m_data.find(section);
	if (it_ext != m_data.end()) {
		auto it_inter = it_ext->second.find(parameter_name);
		if (it_inter != it_ext->second.end())
			return std::get<double>(it_inter->second);
	}

	throw voxen::FormattedMessageException("Double option {}/{} not found", fmt::make_format_args(section, parameter_name));
	return 0.0;
}

int64_t voxen::Config::optionInt(string_view section, string_view parameter_name) const
{
	auto it_ext = m_data.find(section);
	if (it_ext != m_data.end()) {
		auto it_inter = it_ext->second.find(parameter_name);
		if (it_inter != it_ext->second.end())
			return std::get<int64_t>(it_inter->second);
	}

	throw voxen::FormattedMessageException("Int option {}/{} not found", fmt::make_format_args(section, parameter_name));
	return 0;
}

std::string voxen::Config::optionString(string_view section, string_view parameter_name) const
{
	auto it_ext = m_data.find(section);
	if (it_ext != m_data.end()) {
		auto it_inter = it_ext->second.find(parameter_name);
		if (it_inter != it_ext->second.end())
			return std::get<std::string>(it_inter->second);
	}

	throw voxen::FormattedMessageException("String option {}/{} not found", fmt::make_format_args(section, parameter_name));
	return std::string();
}

int voxen::Config::optionType(string_view section, string_view parameter_name) const
{
	auto it_ext = m_data.find(section);
	if (it_ext != m_data.end()) {
		auto it_inter = it_ext->second.find(parameter_name);
		if (it_inter != it_ext->second.end())
			return it_inter->second.index();
	}

	throw voxen::FormattedMessageException("Option {}/{} not found", fmt::make_format_args(section, parameter_name));
	return 0;
}

void voxen::Config::patch(string_view section, string_view parameter_name, option_t value, bool saveToConfigFile)
{
	auto it_ext = m_data.find(section);
	if (it_ext != m_data.end()) {
		auto it_inter = it_ext->second.find(parameter_name);
		if (it_inter != it_ext->second.end()) {
			if (value.index() == it_inter->second.index()) {
				it_inter->second = value;
				if (saveToConfigFile) {
					const std::string& str = Config::optionToString(value);
					voxen::Log::info("{}::{} {}", section, parameter_name, str);
					m_ini.SetValue(section.data(), parameter_name.data(), str.c_str());
				}
				return;
			} else {
				throw voxen::FormattedMessageException(
					"Inconsistent types for option {}/{}: try replace {} with {}", fmt::make_format_args(section, parameter_name, it_inter->second.index(), value.index())
				);
			}
		}
	}

	throw voxen::FormattedMessageException("Option {}/{} not found", fmt::make_format_args(section, parameter_name));
}

std::string voxen::Config::optionToString(option_t value)
{
	using namespace std;

	int type_idx = value.index();
	switch(type_idx) {
		case 0:
			static_assert(is_same_v<string, variant_alternative_t<0, voxen::Config::option_t>>);
			return get<string>(value);

		case 1:
			static_assert(is_same_v<int64_t, variant_alternative_t<1, voxen::Config::option_t>>);
			return to_string(get<int64_t>(value));

		case 2:
			static_assert(is_same_v<double, variant_alternative_t<2, voxen::Config::option_t>>);
			return to_string(get<double>(value));

		case 3:
			static_assert(is_same_v<bool, variant_alternative_t<3, voxen::Config::option_t>>);
			return get<bool>(value) ? "true" : "false";

		default:
			static_assert(std::variant_size_v<voxen::Config::option_t> == 4);
			return "";
	}
}

voxen::Config::option_t voxen::Config::optionFromString(string_view s, int type)
{
	switch(type) {
		case 0:
			static_assert(std::is_same_v<std::string,   std::variant_alternative_t<0, voxen::Config::option_t>>);
			return std::string(s);

		case 1:
			static_assert(std::is_same_v<int64_t,   std::variant_alternative_t<1, voxen::Config::option_t>>);
			return (int64_t)std::stoi(s.data());

		case 2:
			static_assert(std::is_same_v<double,   std::variant_alternative_t<2, voxen::Config::option_t>>);
			return std::stod(s.data());

		case 3:
		{
			static_assert(std::is_same_v<bool,   std::variant_alternative_t<3, voxen::Config::option_t>>);
			if (s.size() != 4)
				return false;
			bool isTrueStr = tolower(s[0]) == 't';
			isTrueStr     &= tolower(s[1]) == 'r';
			isTrueStr     &= tolower(s[2]) == 'u';
			isTrueStr     &= tolower(s[3]) == 'e';
			return isTrueStr;
		}

		default:
			static_assert(std::variant_size_v<voxen::Config::option_t> == 4);
			return "";
	}
}
