#include <voxen/svc/runtime_config.hpp>

#include <voxen/gfx/gfx_options.hpp>

namespace voxen::svc
{

struct detail::RuntimeConfigImpl {
	gfx::GfxOptions gfx_options;
};

namespace
{

RuntimeConfig g_instance;

} // namespace

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
	detail::RuntimeConfigImpl &me = m_impl.object();
	me.gfx_options.fillFromCli(args);
}

RuntimeConfig &RuntimeConfig::instance() noexcept
{
	return g_instance;
}

} // namespace voxen::svc
