#pragma once

#include <voxen/client/vulkan/common.hpp>

namespace voxen::client
{

class VulkanFramebuffer {
public:
	explicit VulkanFramebuffer(const VkFramebufferCreateInfo &info);
	VulkanFramebuffer(VulkanFramebuffer &&) = delete;
	VulkanFramebuffer(const VulkanFramebuffer &) = delete;
	VulkanFramebuffer &operator = (VulkanFramebuffer &&) = delete;
	VulkanFramebuffer &operator = (const VulkanFramebuffer &) = delete;
	~VulkanFramebuffer() noexcept;

	operator VkFramebuffer() const noexcept { return m_framebuffer; }
private:
	VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
};

class VulkanFramebufferCollection {
public:
	VulkanFramebufferCollection();
	VulkanFramebufferCollection(VulkanFramebufferCollection &&) = delete;
	VulkanFramebufferCollection(const VulkanFramebufferCollection &) = delete;
	VulkanFramebufferCollection &operator = (VulkanFramebufferCollection &&) = delete;
	VulkanFramebufferCollection &operator = (const VulkanFramebufferCollection &) = delete;
	~VulkanFramebufferCollection() = default;

	VulkanFramebuffer &sceneFramebuffer() noexcept { return m_scene_framebuffer; }
private:
	VulkanFramebuffer m_scene_framebuffer;

	VulkanFramebuffer createSceneFramebuffer();
};

}
