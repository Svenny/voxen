#include <voxen/common/runtime_config.hpp>

#include <voxen/client/gfx_runtime_config.hpp>

namespace voxen
{

struct RuntimeConfig::Impl {
	client::GfxRuntimeConfig gfx_config;
};

static RuntimeConfig g_instance;

RuntimeConfig::RuntimeConfig() = default;
RuntimeConfig::~RuntimeConfig() noexcept = default;

const client::GfxRuntimeConfig &RuntimeConfig::gfxConfig() const noexcept
{
	return m_impl.object().gfx_config;
}

void RuntimeConfig::addOptions(tk::cli::Parser &parser)
{
	client::GfxRuntimeConfig::addOptions(parser);
}

void RuntimeConfig::fill(const tk::cli::Args &args)
{
	Impl &me = m_impl.object();
	me.gfx_config.fill(args);
}

RuntimeConfig &RuntimeConfig::instance() noexcept
{
	return g_instance;
}

} // namespace voxen
