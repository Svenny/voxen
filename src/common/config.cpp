#include <voxen/common/config.hpp>

#include <fmt/format.h>
#include <voxen/util/exception.hpp>
#include <voxen/util/voxen_home.hpp>
#include <voxen/util/log.hpp>

using namespace voxen;
using namespace std::filesystem;

const path Config::kMainConfigRelPath = "config.ini";
Config* Config::g_instance = nullptr;

Config::Config(path path, Config::Scheme scheme): debug_stored_scheme(scheme){
	(void)path;
}

Config* Config::mainConfig() {
	if(!g_instance) {
		path config_path = voxenHome() / kMainConfigRelPath;
		g_instance = new Config(config_path, mainConfigScheme());
	}
	return g_instance;
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

bool voxen::Config::optionBool(std::string section, std::string parameter_name)
{
	for (SchemeEntry& entry : debug_stored_scheme)
		if (entry.section == section && entry.parameter_name == parameter_name)
			return std::get<bool>(entry.default_value);

	throw voxen::FormattedMessageException("Bool option {}/{} not found", fmt::make_format_args(section, parameter_name));
	return false;
}

double voxen::Config::optionDouble(std::string section, std::string parameter_name)
{
	for (SchemeEntry& entry : debug_stored_scheme)
		if (entry.section == section && entry.parameter_name == parameter_name)
			return std::get<double>(entry.default_value);

	throw voxen::FormattedMessageException("Double option {}/{} not found", fmt::make_format_args(section, parameter_name));
	return 0.0;
}

int64_t voxen::Config::optionInt(std::string section, std::string parameter_name)
{
	for (SchemeEntry& entry : debug_stored_scheme)
		if (entry.section == section && entry.parameter_name == parameter_name)
			return std::get<int64_t>(entry.default_value);

	throw voxen::FormattedMessageException("Int option {}/{} not found", fmt::make_format_args(section, parameter_name));
	return 0;
}

std::string voxen::Config::optionString(std::string section, std::string parameter_name)
{
	for (SchemeEntry& entry : debug_stored_scheme)
		if (entry.section == section && entry.parameter_name == parameter_name)
			return std::get<std::string>(entry.default_value);

	throw voxen::FormattedMessageException("String option {}/{} not found", fmt::make_format_args(section, parameter_name));
	return std::string();
}

int voxen::Config::optionType(std::string section, std::string parameter_name)
{
	for (SchemeEntry& entry : debug_stored_scheme)
		if (entry.section == section && entry.parameter_name == parameter_name)
			return entry.default_value.index();

	throw voxen::FormattedMessageException("Option {}/{} not found", fmt::make_format_args(section, parameter_name));
	return 0;
}

void voxen::Config::patch(std::string section, std::string parameter_name, option_t value)
{
	for (SchemeEntry& entry : debug_stored_scheme)
		if (entry.section == section && entry.parameter_name == parameter_name) {
			if (value.index() == entry.default_value.index()) {
				entry.default_value = value;
				return;
			} else {
				throw voxen::FormattedMessageException(
					"Inconsistent types for option {}/{}: try replace {} with {}", fmt::make_format_args(section, parameter_name, entry.default_value.index(), value.index())
				);
			}
		}

	throw voxen::FormattedMessageException("Option {}/{} not found", fmt::make_format_args(section, parameter_name));
}
