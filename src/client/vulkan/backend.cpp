#include <voxen/client/vulkan/backend.hpp>

#include <voxen/client/vulkan/instance.hpp>
#include <voxen/client/vulkan/device.hpp>
#include <voxen/util/log.hpp>

namespace voxen::client
{

VulkanBackend::~VulkanBackend() noexcept {
	stop();
}

bool VulkanBackend::start() noexcept {
	if (m_state != State::NotStarted) {
		Log::warn("Vulkan backend is already started");
		return true;
	}

	try {
		Log::info("Starting Vulkan backend");
		m_instance = new VulkanInstance(*this);
		m_device = new VulkanDevice(*this);
	}
	catch (const Exception &e) {
		Log::error("An voxen::Exception was catched during starting Vulkan backend");
		Log::error("what(): {}", e.what());
		auto loc = e.where();
		Log::error("where(): {}:{} ({})", loc.file_name(), loc.line(), loc.function_name());
		stop();
		return false;
	}
	catch (const std::exception &e) {
		Log::error("An std::exception was catched during starting Vulkan backend");
		Log::error("what(): {}", e.what());
		stop();
		return false;
	}
	catch (...) {
		Log::error("An unknown exception was catched during starting Vulkan backend");
		stop();
		return false;
	}

	m_state = State::Started;
	return true;
}

void VulkanBackend::stop() noexcept {
	if (m_state == State::NotStarted) {
		Log::warn("Vulkan backend is already stopped");
		return;
	}
	Log::info("Stopping Vulkan backend");

	delete m_device;
	m_device = nullptr;
	delete m_instance;
	m_instance = nullptr;

	m_state = State::NotStarted;
}

}
