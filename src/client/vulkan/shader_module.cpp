#include <voxen/client/vulkan/shader_module.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>

#include <voxen/util/log.hpp>

#include <extras/defer.hpp>
#include <extras/dyn_array.hpp>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace voxen::client
{

VulkanShaderModule::VulkanShaderModule(const char *path) {
	if (!path) {
		Log::error("Null pointer passed as path");
		throw MessageException("null pointer");
	}

	Log::debug("Loading shader module `{}`", path);
	// TODO: handle EINTR
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		throw ErrnoException(errno, "open");
	defer { close(fd); };

	struct stat file_stat;
	if (fstat(fd, &file_stat) != 0)
		throw ErrnoException(errno, "fstat");

	size_t code_size = file_stat.st_size;
	extras::dyn_array<std::byte> code_bytes(code_size);
	// TODO: handle EINTR
	if (read(fd, code_bytes.data(), code_size) < 0)
		throw ErrnoException(errno, "read");

	VkShaderModuleCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = code_size;
	info.pCode = reinterpret_cast<const uint32_t *>(code_bytes.data());

	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	VkResult result = backend.vkCreateShaderModule(device, &info, VulkanHostAllocator::callbacks(), &m_shader_module);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkCreateShaderModule");
}

VulkanShaderModule::~VulkanShaderModule() noexcept {
	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	backend.vkDestroyShaderModule(device, m_shader_module, VulkanHostAllocator::callbacks());
}

VulkanShaderModuleCollection::VulkanShaderModuleCollection() :
// TODO: use proper paths
	m_debug_octree_vertex("vert.spv"),
	m_debug_octree_fragment("frag.spv")
{
	Log::debug("VulkanShaderModuleCollection created sucessfully");
}

}
