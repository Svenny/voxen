#pragma once

#include <voxen/client/vulkan/common.hpp>

namespace voxen::client::vulkan
{

class Instance {
public:
	Instance();
	Instance(Instance &&) = delete;
	Instance(const Instance &) = delete;
	Instance &operator = (Instance &&) = delete;
	Instance &operator = (const Instance &) = delete;
	~Instance() noexcept;

	operator VkInstance() const noexcept { return m_handle; }
private:
	VkInstance m_handle = VK_NULL_HANDLE;

	bool checkVulkanSupport() const;
	void createInstance();
	void destroyInstance() noexcept;
};

}
