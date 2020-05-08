set (VOXEN_SOURCES
	src/client/vulkan/base.cpp
	src/client/vulkan/buffer_manager.cpp
	src/client/vulkan/command_manager.cpp
	src/client/vulkan/common.cpp
	src/client/vulkan/descriptor_manager.cpp
	src/client/vulkan/memory_allocator.cpp
	src/client/vulkan/pipeline_cache.cpp
	src/client/vulkan/pipeline_manager.cpp
	src/client/vulkan/render_pass_manager.cpp
	src/client/vulkan/shader.cpp
	src/client/vulkan/swapchain.cpp
	src/client/vulkan/texture_manager.cpp
	src/client/vulkan/vulkan_render.cpp
	src/client/debug_draw_octree.cpp
	src/client/input_manager.cpp
	src/client/render.cpp
	src/client/window.cpp
	src/common/player.cpp
	src/common/terrain.cpp
	src/common/world.cpp
	src/common/gui.cpp
	src/common/gameview.cpp
	src/common/config.cpp
	src/util/assert.cpp
	src/util/exception.cpp
	src/util/log.cpp
	src/main.cpp
)

target_sources (voxen PUBLIC ${VOXEN_SOURCES})
source_group (TREE ${CMAKE_SOURCE_DIR}/src PREFIX Sources FILES ${VOXEN_SOURCES})
