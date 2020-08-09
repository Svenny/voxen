#include <voxen/client/vulkan/shader_module.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>

#include <voxen/util/file.hpp>
#include <voxen/util/log.hpp>

namespace voxen::client::vulkan
{

ShaderModule::ShaderModule(const char *path)
{
	load(path);
}

void ShaderModule::load(const char *path)
{
	unload();

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

	auto &backend = Backend::backend();
	VkDevice device = *backend.device();
	VkResult result = backend.vkCreateShaderModule(device, &info, VulkanHostAllocator::callbacks(), &m_shader_module);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkCreateShaderModule");
}

void ShaderModule::unload() noexcept
{
	if (!isLoaded())
		return;

	auto &backend = Backend::backend();
	VkDevice device = *backend.device();
	backend.vkDestroyShaderModule(device, m_shader_module, VulkanHostAllocator::callbacks());
	m_shader_module = VK_NULL_HANDLE;
}

ShaderModule::~ShaderModule() noexcept
{
	unload();
}

// This won't compile if you forget to add path
// here after adding new shader id to the enum
template<uint32_t ID>
extern const char *SHADER_MODULE_PATH;
#define ADD_SHADER_PATH(name, path) \
	template<> inline const char *SHADER_MODULE_PATH<ShaderModuleCollection::name> = path;

ADD_SHADER_PATH(DEBUG_OCTREE_VERTEX, "assets/shaders/debug/octree.vert.spv")
ADD_SHADER_PATH(DEBUG_OCTREE_FRAGMENT, "assets/shaders/debug/octree.frag.spv")

template<uint32_t ID = 0, typename T>
static void loadShaderModules(T &array)
{
	// Essentially a loop over all registered shaders ids
	if constexpr (ID < ShaderModuleCollection::NUM_SHADERS) {
		array[ID].load(SHADER_MODULE_PATH<ID>);
		loadShaderModules<ID + 1>(array);
	}
}

ShaderModuleCollection::ShaderModuleCollection()
{
	Log::debug("Creating ShaderModuleCollection");
	loadShaderModules(m_shader_modules);
	Log::debug("ShaderModuleCollection created sucessfully");
}

const ShaderModule &ShaderModuleCollection::operator[](ShaderId idx) const
{
	return m_shader_modules.at(idx);
}

}
