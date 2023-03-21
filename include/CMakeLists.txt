set(EXTRAS_HEADERS
	include/extras/bitset.hpp
	include/extras/defer.hpp
	include/extras/dyn_array.hpp
	include/extras/enum_utils.hpp
	include/extras/fixed_pool.hpp
	include/extras/function_ref.hpp
	include/extras/futex.hpp
	include/extras/hardware_params.hpp
	include/extras/linear_allocator.hpp
	include/extras/math.hpp
	include/extras/pimpl.hpp
	include/extras/refcnt_ptr.hpp
	include/extras/source_location.hpp
	include/extras/string_utils.hpp
)

target_include_directories(extras PUBLIC include)
target_sources(extras PRIVATE ${EXTRAS_HEADERS})
source_group(TREE ${CMAKE_SOURCE_DIR}/include/extras PREFIX Headers FILES ${EXTRAS_HEADERS})

set(VOXEN_HEADERS
	include/voxen/client/vulkan/algo/terrain_renderer.hpp
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
	include/voxen/client/vulkan/config.hpp
	include/voxen/client/vulkan/descriptor_manager.hpp
	include/voxen/client/vulkan/descriptor_set_layout.hpp
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
	include/voxen/common/assets/png_tools.hpp
	include/voxen/common/msg/config.hpp
	include/voxen/common/msg/message.hpp
	include/voxen/common/msg/queue.hpp
	include/voxen/common/terrain/allocator.hpp
	include/voxen/common/terrain/cache.hpp
	include/voxen/common/terrain/chunk.hpp
	include/voxen/common/terrain/chunk_id.hpp
	include/voxen/common/terrain/config.hpp
	include/voxen/common/terrain/controller.hpp
	include/voxen/common/terrain/control_block.hpp
	include/voxen/common/terrain/coord.hpp
	include/voxen/common/terrain/generator.hpp
	include/voxen/common/terrain/hermite_data.hpp
	include/voxen/common/terrain/loader.hpp
	include/voxen/common/terrain/primary_data.hpp
	include/voxen/common/terrain/qef_solver.hpp
	include/voxen/common/terrain/surface.hpp
	include/voxen/common/terrain/surface_builder.hpp
	include/voxen/common/terrain/voxel_grid.hpp
	include/voxen/common/player.hpp
	include/voxen/common/threadpool.hpp
	include/voxen/common/world_state.hpp
	include/voxen/common/gameview.hpp
	include/voxen/common/config.hpp
	include/voxen/common/filemanager.hpp
	include/voxen/server/world.hpp
	include/voxen/util/aabb.hpp
	include/voxen/util/allocator.hpp
	include/voxen/util/debug.hpp
	include/voxen/util/elapsed_timer.hpp
	include/voxen/util/error_condition.hpp
	include/voxen/util/exception.hpp
	include/voxen/util/hash.hpp
	include/voxen/util/log.hpp
	include/voxen/util/resolution.hpp
	include/voxen/visibility.hpp
)

target_include_directories(voxen PUBLIC include)
target_sources(voxen PRIVATE ${VOXEN_HEADERS})
source_group(TREE ${CMAKE_SOURCE_DIR}/include/voxen PREFIX Headers FILES ${VOXEN_HEADERS})

# Generate build-config header
configure_file(include/voxen/config.hpp.cmake ${CMAKE_CURRENT_BINARY_DIR}/configs/voxen/config.hpp)

# Generate version header
add_custom_target(voxen-version-file
	COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/cmake/gen-version-header.py
		${CMAKE_BINARY_DIR}/configs/voxen/version.hpp
		${VOXEN_VERSION_MAJOR} ${VOXEN_VERSION_MINOR} ${VOXEN_VERSION_PATCH} ${VOXEN_VERSION_SUFFIX}
	BYPRODUCTS ${CMAKE_BINARY_DIR}/configs/voxen/version.hpp
	COMMENT "Generating version.hpp file"
)
add_dependencies(voxen voxen-version-file)

# Include directory with generated headers
target_include_directories(voxen PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/configs)
