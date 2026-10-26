#include <voxen/common/runtime_config.hpp>

#include <voxen/gfx/gfx_options.hpp>

namespace voxen
{

struct RuntimeConfig::Impl {
	gfx::GfxOptions gfx_options;
};

static RuntimeConfig g_instance;

RuntimeConfig::RuntimeConfig() = default;
RuntimeConfig::~RuntimeConfig() noexcept = default;

const gfx::GfxOptions &RuntimeConfig::gfxOptions() const noexcept
{
	return m_impl.object().gfx_options;
}

void RuntimeConfig::addOptions(tk::cli::Parser &parser)
{
	gfx::GfxOptions::addCliOptions(parser);
}

void RuntimeConfig::fill(const tk::cli::Args &args)
{
	Impl &me = m_impl.object();
	me.gfx_options.fillFromCli(args);
}

RuntimeConfig &RuntimeConfig::instance() noexcept
{
	return g_instance;
}

} // namespace voxen
