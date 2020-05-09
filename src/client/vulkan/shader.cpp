#include <voxen/client/vulkan/shader.hpp>
#include <voxen/client/vulkan/common.hpp>
#include <voxen/util/log.hpp>

#include <extras/dyn_array.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace voxen
{

VulkanShader::VulkanShader(VkDevice dev) noexcept : m_dev(dev) {}

VulkanShader::~VulkanShader() noexcept {
	vkDestroyShaderModule(m_dev, m_module, VulkanHostAllocator::callbacks());
}

bool VulkanShader::load(const char *path) {
	if (!path) {
		Log::error("Null pointer passed as path");
		return false;
	}

	// TODO: handle EINTR
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		int code = errno;
		char buf[128];
		Log::error("Failed to open `{}` (error {}: {})", path, code, strerror_r(code, buf, 128));
		return false;
	}
	struct stat file_stat;
	if (fstat(fd, &file_stat) != 0) {
		int code = errno;
		char buf[128];
		Log::error("Failed to stat `{}` (error {}: {})", path, code, strerror_r(code, buf, 128));
		close(fd);
		return false;
	}
	size_t code_size = file_stat.st_size;
	extras::dyn_array<std::byte> code_bytes(code_size);
	// TODO: handle EINTR
	if (read(fd, code_bytes.data(), code_size) < 0) {
		int code = errno;
		char buf[128];
		Log::error("Failed to read `{}` (error {}: {})", path, code, strerror_r(code, buf, 128));
		close(fd);
		return false;
	}
	close(fd);

	VkShaderModuleCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = code_size;
	create_info.pCode = reinterpret_cast<const uint32_t *>(code_bytes.data());
	VkResult result = vkCreateShaderModule(m_dev, &create_info, VulkanHostAllocator::callbacks(), &m_module);
	if (result != VK_SUCCESS) {
		Log::error("Failed to create shader module, error {} ({})");
		return false;
	}
	return true;
}

}
