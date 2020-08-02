#include <voxen/client/vulkan/shader_module.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>

#include <voxen/util/file.hpp>
#include <voxen/util/log.hpp>

namespace voxen::client::vulkan
{

ShaderModule::ShaderModule(const char *path) {
	if (!path) {
		Log::error("Null pointer passed as path");
		throw MessageException("null pointer");
	}

	Log::debug("Loading shader module `{}`", path);
	auto code = FileUtils::readFile(path);
	auto &code_bytes = code.value();

	VkShaderModuleCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = code_bytes.size();
	info.pCode = reinterpret_cast<const uint32_t *>(code_bytes.data());

	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	VkResult result = backend.vkCreateShaderModule(device, &info, VulkanHostAllocator::callbacks(), &m_shader_module);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkCreateShaderModule");
}

ShaderModule::~ShaderModule() noexcept {
	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	backend.vkDestroyShaderModule(device, m_shader_module, VulkanHostAllocator::callbacks());
}

ShaderModuleCollection::ShaderModuleCollection() :
// TODO: use proper paths
	m_debug_octree_vertex("vert.spv"),
	m_debug_octree_fragment("frag.spv")
{
	Log::debug("ShaderModuleCollection created sucessfully");
}

}
