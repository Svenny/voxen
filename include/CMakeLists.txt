set(EXTRAS_HEADERS
	include/extras/defer.hpp
	include/extras/dyn_array.hpp
	include/extras/math.hpp
)

set(VOXEN_HEADERS
	include/voxen/client/vulkan/backend.hpp
	include/voxen/client/vulkan/buffer_manager.hpp
	include/voxen/client/vulkan/command_buffer.hpp
	include/voxen/client/vulkan/command_pool.hpp
	include/voxen/client/vulkan/common.hpp
	include/voxen/client/vulkan/descriptor_manager.hpp
	include/voxen/client/vulkan/device.hpp
	include/voxen/client/vulkan/instance.hpp
	include/voxen/client/vulkan/memory_allocator.hpp
	include/voxen/client/vulkan/pipeline_cache.hpp
	include/voxen/client/vulkan/pipeline_manager.hpp
	include/voxen/client/vulkan/queue_manager.hpp
	include/voxen/client/vulkan/render_pass_manager.hpp
	include/voxen/client/vulkan/shader.hpp
	include/voxen/client/vulkan/swapchain.hpp
	include/voxen/client/vulkan/texture_manager.hpp
	include/voxen/client/vulkan/vulkan_render.hpp
	include/voxen/client/debug_draw_octree.hpp
	include/voxen/client/gui.hpp
	include/voxen/client/input_manager.hpp
	include/voxen/client/render.hpp
	include/voxen/client/window.hpp
	include/voxen/common/player.hpp
	include/voxen/common/terrain.hpp
	include/voxen/common/world.hpp
	include/voxen/common/gameview.hpp
	include/voxen/common/config.hpp
	include/voxen/util/assert.hpp
	include/voxen/util/exception.hpp
	include/voxen/util/log.hpp
)

target_include_directories(voxen PUBLIC include)
target_sources(voxen PUBLIC ${VOXEN_HEADERS} ${EXTRAS_HEADERS})
source_group(TREE ${CMAKE_SOURCE_DIR}/include/voxen PREFIX Headers FILES ${VOXEN_HEADERS})
source_group(TREE ${CMAKE_SOURCE_DIR}/include/extras PREFIX Headers FILES ${EXTRAS_HEADERS})

configure_file(include/voxen/config.hpp.cmake ${CMAKE_CURRENT_BINARY_DIR}/configs/voxen/config.hpp)
target_include_directories(voxen PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/configs)
