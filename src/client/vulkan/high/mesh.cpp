#include <voxen/client/vulkan/high/mesh.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/high/transfer_manager.hpp>

#include <voxen/util/log.hpp>

namespace voxen::client::vulkan
{

VkDeviceSize getVertexElementSize(VertexFormat fmt) noexcept
{
	switch (fmt) {
	case VertexFormat::Nothing:
		return 0;
	case VertexFormat::Pos3D:
		return 3 * 4;
	case VertexFormat::Pos3D_Norm3D:
		return 3 * 4 + 3 * 4;
	} // No `default` to make `-Werror -Wswitch` protection work
}

VkDeviceSize getIndexElementSize(IndexFormat fmt) noexcept
{
	switch (fmt) {
	case IndexFormat::Nothing:
		return 0;
	case IndexFormat::Index8:
		return 1;
	case IndexFormat::Index16:
		return 2;
	case IndexFormat::Index32:
		return 4;
	} // No `default` to make `-Werror -Wswitch` protection work
}

VkIndexType getIndexType(IndexFormat fmt) noexcept
{
	switch (fmt) {
	case IndexFormat::Nothing:
		return VK_INDEX_TYPE_NONE_KHR;
	case IndexFormat::Index8:
		return VK_INDEX_TYPE_UINT8_EXT;
	case IndexFormat::Index16:
		return VK_INDEX_TYPE_UINT16;
	case IndexFormat::Index32:
		return VK_INDEX_TYPE_UINT32;
	} // No `default` to make `-Werror -Wswitch` protection work
}

Mesh::Mesh(const MeshCreateInfo &create_info)
	: m_vertex_format(create_info.vertex_format), m_index_format(create_info.index_format)
{
	// Sanity checks - there must be no vertices/indices with Nothing format and non-zero otherwise
	assert(m_vertex_format == VertexFormat::Nothing ? create_info.num_vertices == 0 : create_info.num_vertices > 0);
	assert(m_index_format == IndexFormat::Nothing ? create_info.num_indices == 0 : create_info.num_indices > 0);

	TransferManager &transfer = Backend::backend().transferManager();

	if (m_vertex_format != VertexFormat::Nothing) {
		VkDeviceSize vertex_buffer_size = getVertexElementSize(m_vertex_format) * create_info.num_vertices;
		VkBufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.size = vertex_buffer_size;
		info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		m_vertex_buffer.emplace(info, FatVkBuffer::Usage::DeviceLocal);

		transfer.uploadToBuffer(m_vertex_buffer.value(), create_info.vertex_data, vertex_buffer_size);
	}

	if (m_index_format != IndexFormat::Nothing) {
		VkDeviceSize index_buffer_size = getIndexElementSize(m_index_format) * create_info.num_indices;
		VkBufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.size = index_buffer_size;
		info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		m_index_buffer.emplace(info, FatVkBuffer::Usage::DeviceLocal);

		transfer.uploadToBuffer(m_index_buffer.value(), create_info.index_data, index_buffer_size);
	}

	// TODO: do this after all meshes are loaded
	transfer.ensureUploadsDone();
}

void Mesh::bindBuffers(VkCommandBuffer cmd_buffer)
{
	auto &backend = Backend::backend();

	if (m_vertex_buffer.has_value()) {
		VkBuffer vertex_buffer = m_vertex_buffer.value();
		VkDeviceSize offset = 0;
		backend.vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &vertex_buffer, &offset);
	}
	if (m_index_buffer.has_value()) {
		backend.vkCmdBindIndexBuffer(cmd_buffer, m_index_buffer.value(), 0, getIndexType(m_index_format));
	}
}

} // namespace voxen::client::vulkan
