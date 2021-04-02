#include <voxen/client/vulkan/shader_module.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>

#include <voxen/common/filemanager.hpp>
#include <voxen/util/log.hpp>

namespace voxen::client::vulkan
{

ShaderModule::ShaderModule(const char *relative_path)
{
	load(relative_path);
}

void ShaderModule::load(const char *relative_path)
{
	unload();

	if (!relative_path) {
		Log::error("Null pointer passed as path");
		throw MessageException("null pointer");
	}

	Log::debug("Loading shader module `{}`", relative_path);
	auto code = FileManager::readFile(relative_path);
	auto &code_bytes = code.value();

	VkShaderModuleCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = code_bytes.size();
	info.pCode = reinterpret_cast<const uint32_t *>(code_bytes.data());

	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	VkResult result = backend.vkCreateShaderModule(device, &info, VulkanHostAllocator::callbacks(), &m_shader_module);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkCreateShaderModule");
}

void ShaderModule::unload() noexcept
{
	if (!isLoaded())
		return;

	auto &backend = Backend::backend();
	VkDevice device = backend.device();
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
ADD_SHADER_PATH(TERRAIN_SIMPLE_VERTEX, "assets/shaders/terrain/simple.vert.spv")
ADD_SHADER_PATH(TERRAIN_SIMPLE_FRAGMENT, "assets/shaders/terrain/simple.frag.spv")

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
