#pragma once

#include <voxen/client/window.hpp>
#include <voxen/client/debug_draw_octree.hpp>
#include <voxen/common/player.hpp>

namespace voxen::client
{

class VulkanRender {
public:
	VulkanRender(Window &w);
	~VulkanRender();

	void beginFrame();
	void debugDrawOctreeNode(glm::mat4 camera_matrix, float base_x, float base_y, float base_z, float size);
	void endFrame();
private:
	class VulkanImpl *m_vk = nullptr;

	DebugDrawOctree *m_octree = nullptr;
};

}
