#include <voxen/gfx/gfx_system.hpp>

#include <voxen/client/gfx_runtime_config.hpp>
#include <voxen/common/runtime_config.hpp>
#include <voxen/gfx/frame_tick_source.hpp>
#include <voxen/gfx/vk/render_graph_runner.hpp>
#include <voxen/gfx/vk/vk_device.hpp>
#include <voxen/gfx/vk/vk_instance.hpp>
#include <voxen/gfx/vk/vk_physical_device.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

namespace voxen::gfx
{

namespace
{

vk::PhysicalDevice *selectGpu(std::span<gfx::vk::PhysicalDevice> gpus)
{
	if (gpus.empty()) {
		Log::error("No Vulkan devices available");
		return nullptr;
	}

	// Choose in this order:
	// 1. User-preferred GPU, if found
	// 2. The first listed discrete GPU
	// 3. If no discrete GPUs, then any integrated one
	// 4. If no integrated GPUs, then just the first listed device
	//
	// Don't do any complex "score" calculations, the user
	// will select GPU manually if he has some unusual setup.
	vk::PhysicalDevice *preferred = nullptr;
	vk::PhysicalDevice *discrete = nullptr;
	vk::PhysicalDevice *integrated = nullptr;
	vk::PhysicalDevice *other = nullptr;

	Log::debug("Searching for Vulkan device");
	for (auto &gpu : gpus) {
		std::string_view name = gpu.info().props.properties.deviceName;

		if (!vk::Device::isSupported(gpu)) {
			Log::debug("'{}' is does not pass minimal requirements", name);
			continue;
		}

		if (name == RuntimeConfig::instance().gfxConfig().preferredGpuName()) {
			Log::debug("'{}' is preferred in config, taking it", name);
			preferred = &gpu;
			break;
		}

		VkPhysicalDeviceType type = gpu.info().props.properties.deviceType;

		if (type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			Log::debug("'{}' is dGPU, taking it", name);
			discrete = &gpu;
			break;
		}

		if (type == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
			Log::debug("'{}' is iGPU, might take it if won't find dGPU", name);
			integrated = &gpu;
		} else if (!other) {
			Log::debug("'{}' is neither iGPU nor dGPU, might take it if won't find one", name);
			other = &gpu;
		}
	}

	vk::PhysicalDevice *chosen = preferred;
	if (!chosen) {
		chosen = discrete ? discrete : (integrated ? integrated : other);
	}

	if (!chosen) {
		Log::error("No Vulkan devices passing minimal requirements found");
		return nullptr;
	}

	Log::debug("Selected GPU: '{}'", chosen->info().props.properties.deviceName);
	return chosen;
}

} // namespace

struct GfxSystem::ComponentStorage {
	alignas(vk::Instance) std::byte vk_instance[sizeof(vk::Instance)];
	alignas(vk::Device) std::byte vk_device[sizeof(vk::Device)];
	alignas(vk::RenderGraphRunner) std::byte vk_render_graph_runner[sizeof(vk::RenderGraphRunner)];
	alignas(FrameTickSource) std::byte frame_tick_source[sizeof(FrameTickSource)];
};

GfxSystem::GfxSystem(svc::ServiceLocator &svc, os::GlfwWindow &main_window)
	: m_component_storage(std::make_unique<ComponentStorage>())
{
	Log::info("Starting gfx system");

	// TODO: once options service is implemented, use it instead of `RuntimeConfig`
	(void) svc;

	ComponentStorage &comp = *m_component_storage;
	m_vk_instance.reset(new (comp.vk_instance) vk::Instance());

	auto gpus = m_vk_instance->enumeratePhysicalDevices();
	vk::PhysicalDevice *selected_gpu = selectGpu(gpus);
	if (!selected_gpu) {
		throw Exception::fromError(VoxenErrc::GfxCapabilityMissing, "no supported GPU found");
	}

	m_vk_device.reset(new (comp.vk_device) vk::Device(*m_vk_instance, *selected_gpu));
	m_vk_render_graph_runner.reset(new (comp.vk_render_graph_runner) vk::RenderGraphRunner(*m_vk_device, main_window));

	m_frame_tick_source.reset(new (comp.frame_tick_source) FrameTickSource());

	// Do tick notification to correctly set the initial tick in every component
	notifyFrameTickBegin(m_frame_tick_source->completedTickId(), m_frame_tick_source->currentTickId());

	Log::info("Started gfx system");
}

GfxSystem::~GfxSystem()
{
	Log::info("Stopping gfx system");
}

void GfxSystem::drawFrame()
{
	auto [completed_tick_id, this_tick_id] = m_frame_tick_source->startNextTick(*this);
	notifyFrameTickBegin(completed_tick_id, this_tick_id);

	// TODO: get current world state, run render graph etc.

	notifyFrameTickEnd(this_tick_id);
}

void GfxSystem::waitFrameCompletion(FrameTickId tick_id)
{
	m_frame_tick_source->waitTickCompletion(*this, tick_id);
}

void GfxSystem::notifyFrameTickBegin(FrameTickId completed_tick, FrameTickId new_tick)
{
	// Order components from more general to more specific ones
	m_vk_device->onFrameTickBegin(completed_tick, new_tick);
}

void GfxSystem::notifyFrameTickEnd(FrameTickId current_tick)
{
	// Reverse order of `notifyFrameTickBegin`, from specific to more general ones
	m_vk_device->onFrameTickEnd(current_tick);
}

} // namespace voxen::gfx
