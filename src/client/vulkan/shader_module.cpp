#include <voxen/client/vulkan/shader_module.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/common/filemanager.hpp>
#include <voxen/gfx/vk/vk_device.hpp>
#include <voxen/gfx/vk/vk_error.hpp>
#include <voxen/util/log.hpp>

namespace voxen::client::vulkan
{

ShaderModule::ShaderModule(std::string_view relative_path)
{
	load(relative_path);
}

void ShaderModule::load(std::string_view relative_path)
{
	unload();

	Log::debug("Loading shader module `{}`", relative_path);
	auto code = FileManager::readFile(relative_path);
	auto &code_bytes = code.value();

	VkShaderModuleCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = code_bytes.size();
	info.pCode = reinterpret_cast<const uint32_t *>(code_bytes.data());

	auto &backend = Backend::backend();
	VkDevice device = backend.device().handle();
	VkResult result = backend.vkCreateShaderModule(device, &info, nullptr, &m_shader_module);
	if (result != VK_SUCCESS) {
		throw gfx::vk::VulkanException(result, "vkCreateShaderModule");
	}
}

void ShaderModule::unload() noexcept
{
	if (!isLoaded()) {
		return;
	}

	auto &backend = Backend::backend();
	VkDevice device = backend.device().handle();
	backend.vkDestroyShaderModule(device, m_shader_module, nullptr);
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
	template<> \
	inline const char *SHADER_MODULE_PATH<ShaderModuleCollection::name> = path;

ADD_SHADER_PATH(LAND_DEBUG_CHUNK_BOUNDS_VERTEX, "assets/shaders/land/debug_chunk_bounds.vert.spv")
ADD_SHADER_PATH(LAND_DEBUG_CHUNK_BOUNDS_FRAGMENT, "assets/shaders/land/debug_chunk_bounds.frag.spv")
ADD_SHADER_PATH(LAND_FRUSTUM_CULL_COMPUTE, "assets/shaders/land/frustum_cull.comp.spv")
ADD_SHADER_PATH(LAND_CHUNK_MESH_VERTEX, "assets/shaders/land/chunk_mesh.vert.spv")
ADD_SHADER_PATH(LAND_CHUNK_MESH_FRAGMENT, "assets/shaders/land/chunk_mesh.frag.spv")
ADD_SHADER_PATH(LAND_SELECTOR_VERTEX, "assets/shaders/land/selector.vert.spv")
ADD_SHADER_PATH(LAND_SELECTOR_FRAGMENT, "assets/shaders/land/selector.frag.spv")
ADD_SHADER_PATH(UI_FONT_VERTEX, "assets/shaders/ui/font.vert.spv")
ADD_SHADER_PATH(UI_FONT_FRAGMENT, "assets/shaders/ui/font.frag.spv")

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

} // namespace voxen::client::vulkan
