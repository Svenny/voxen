set(EXTRAS_HEADERS
	include/extras/bitset.hpp
	include/extras/defer.hpp
	include/extras/dyn_array.hpp
	include/extras/enum_utils.hpp
	include/extras/fixed_pool.hpp
	include/extras/function_ref.hpp
	include/extras/math.hpp
	include/extras/spinlock.hpp
	include/extras/string_utils.hpp
)

set(VOXEN_HEADERS
	include/voxen/client/vulkan/algo/debug_octree.hpp
	include/voxen/client/vulkan/algo/terrain_simple.hpp
	include/voxen/client/vulkan/high/main_loop.hpp
	include/voxen/client/vulkan/high/mesh.hpp
	include/voxen/client/vulkan/high/terrain_synchronizer.hpp
	include/voxen/client/vulkan/high/transfer_manager.hpp
	include/voxen/client/vulkan/backend.hpp
	include/voxen/client/vulkan/buffer.hpp
	include/voxen/client/vulkan/capabilities.hpp
	include/voxen/client/vulkan/command_buffer.hpp
	include/voxen/client/vulkan/command_pool.hpp
	include/voxen/client/vulkan/common.hpp
	include/voxen/client/vulkan/descriptor_manager.hpp
	include/voxen/client/vulkan/device.hpp
	include/voxen/client/vulkan/framebuffer.hpp
	include/voxen/client/vulkan/image.hpp
	include/voxen/client/vulkan/image_view.hpp
	include/voxen/client/vulkan/instance.hpp
	include/voxen/client/vulkan/memory.hpp
	include/voxen/client/vulkan/physical_device.hpp
	include/voxen/client/vulkan/pipeline.hpp
	include/voxen/client/vulkan/pipeline_cache.hpp
	include/voxen/client/vulkan/pipeline_layout.hpp
	include/voxen/client/vulkan/render_pass.hpp
	include/voxen/client/vulkan/shader_module.hpp
	include/voxen/client/vulkan/surface.hpp
	include/voxen/client/vulkan/swapchain.hpp
	include/voxen/client/vulkan/sync.hpp
	include/voxen/client/vulkan/texture_manager.hpp
	include/voxen/client/gui.hpp
	include/voxen/client/render.hpp
	include/voxen/client/window.hpp
	include/voxen/client/input_event_adapter.hpp
	include/voxen/client/player_action_events.hpp
	include/voxen/common/terrain/cache.hpp
	include/voxen/common/terrain/chunk.hpp
	include/voxen/common/terrain/chunk_data.hpp
	include/voxen/common/terrain/chunk_header.hpp
	include/voxen/common/terrain/chunk_octree.hpp
	include/voxen/common/terrain/generator.hpp
	include/voxen/common/terrain/hermite_data.hpp
	include/voxen/common/terrain/loader.hpp
	include/voxen/common/terrain/octree_tables.hpp
	include/voxen/common/terrain/qef_solver.hpp
	include/voxen/common/terrain/seam.hpp
	include/voxen/common/terrain/surface.hpp
	include/voxen/common/terrain/surface_builder.hpp
	include/voxen/common/terrain/types.hpp
	include/voxen/common/player.hpp
	include/voxen/common/terrain.hpp
	include/voxen/common/threadpool.hpp
	include/voxen/common/world_state.hpp
	include/voxen/common/gameview.hpp
	include/voxen/common/config.hpp
	include/voxen/common/filemanager.hpp
	include/voxen/server/world.hpp
	include/voxen/util/elapsed_timer.hpp
	include/voxen/util/exception.hpp
	include/voxen/util/hash.hpp
	include/voxen/util/log.hpp
)

target_include_directories(voxen PUBLIC include)
target_sources(voxen PUBLIC ${VOXEN_HEADERS} ${EXTRAS_HEADERS})
source_group(TREE ${CMAKE_SOURCE_DIR}/include/voxen PREFIX Headers FILES ${VOXEN_HEADERS})
source_group(TREE ${CMAKE_SOURCE_DIR}/include/extras PREFIX Headers FILES ${EXTRAS_HEADERS})

configure_file(include/voxen/config.hpp.cmake ${CMAKE_CURRENT_BINARY_DIR}/configs/voxen/config.hpp)
target_include_directories(voxen PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/configs)
