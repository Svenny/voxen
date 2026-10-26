#pragma once

#include <voxen/gfx/gfx_fwd.hpp>
#include <voxen/gfx/vk/vk_include.hpp>

#include <string_view>

namespace voxen::client::vulkan
{

class PipelineCollection;
class ShaderModuleCollection;

class Backend {
public:
	struct Impl;

	enum class State {
		NotStarted,
		Started,
		Broken,
	};

	bool start(gfx::GfxSystem &gfx) noexcept;
	void stop() noexcept;

	State state() const noexcept { return m_state; }

	gfx::GfxSystem &gfxSystem() noexcept { return *m_gfx_system; }
	gfx::vk::Instance &instance() noexcept { return *m_instance; }
	const gfx::vk::Instance &instance() const noexcept { return *m_instance; }
	gfx::vk::Device &device() noexcept { return *m_device; }
	const gfx::vk::Device &device() const noexcept { return *m_device; }

	ShaderModuleCollection &shaderModuleCollection() noexcept { return *m_shader_module_collection; }
	const ShaderModuleCollection &shaderModuleCollection() const noexcept { return *m_shader_module_collection; }

	PipelineCollection &pipelineCollection() noexcept { return *m_pipeline_collection; }
	const PipelineCollection &pipelineCollection() const noexcept { return *m_pipeline_collection; }

	// Backend is the only Vulkan-related class designed
	// to be a singleton. The main arguments for this decision:
	// 1. Launching multiple backends makes no sense
	// 2. With non-singleton design each downstream entity would have to store reference
	//    to its backend, needlessly (because of p.1) increasing objects' and code size
	// 3. There is a lot of downstream entities (which strengthens p.2)
	static Backend &backend() noexcept { return s_instance; }
	static const Backend &cbackend() noexcept { return s_instance; }

private:
	State m_state = State::NotStarted;

	Impl &m_impl;

	gfx::GfxSystem *m_gfx_system = nullptr;
	gfx::vk::Instance *m_instance = nullptr;
	gfx::vk::Device *m_device = nullptr;

	ShaderModuleCollection *m_shader_module_collection = nullptr;

	PipelineCollection *m_pipeline_collection = nullptr;

	static constinit Backend s_instance;

	static std::string_view stateToString(State state) noexcept;

	bool doStart(gfx::GfxSystem &gfx) noexcept;
	void doStop() noexcept;

	constexpr Backend(Impl &impl) noexcept;
	Backend(Backend &&) = delete;
	Backend(const Backend &) = delete;
	Backend &operator=(Backend &&) = delete;
	Backend &operator=(const Backend &) = delete;
	~Backend() noexcept;
};

} // namespace voxen::client::vulkan
