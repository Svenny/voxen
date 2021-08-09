#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/client/vulkan/buffer.hpp>

#include <optional>

namespace voxen::client::vulkan
{

enum class VertexFormat {
	Nothing,
	Pos3D,
	Pos3D_Norm3D
};

VkDeviceSize getVertexElementSize(VertexFormat fmt) noexcept;

enum class IndexFormat {
	Nothing,
	Index8,
	Index16,
	Index32
};

VkDeviceSize getIndexElementSize(IndexFormat fmt) noexcept;
VkIndexType getIndexType(IndexFormat fmt) noexcept;

struct MeshCreateInfo {
	VertexFormat vertex_format;
	VkDeviceSize num_vertices;
	const void *vertex_data;
	IndexFormat index_format;
	VkDeviceSize num_indices;
	const void *index_data;
};

class Mesh {
public:
	explicit Mesh(const MeshCreateInfo &create_info);
	Mesh(Mesh &&) = delete;
	Mesh(const Mesh &&) = delete;
	Mesh &operator = (Mesh &&) = delete;
	Mesh &operator = (const Mesh &) = delete;
	~Mesh() = default;

	void bindBuffers(VkCommandBuffer cmd_buffer);
private:
	const VertexFormat m_vertex_format;
	const IndexFormat m_index_format;
	std::optional<FatVkBuffer> m_vertex_buffer;
	std::optional<FatVkBuffer> m_index_buffer;
};

}
