set(VOXEN_SOURCES
	src/client/vulkan/algo/terrain_renderer.cpp
	src/client/vulkan/high/terrain_synchronizer.cpp
	src/client/vulkan/high/transfer_manager.cpp
	src/client/vulkan/backend.cpp
	src/client/vulkan/buffer.cpp
	src/client/vulkan/capabilities.cpp
	src/client/vulkan/command_buffer.cpp
	src/client/vulkan/command_pool.cpp
	src/client/vulkan/config.cpp
	src/client/vulkan/descriptor_set_layout.cpp
	src/client/vulkan/pipeline.cpp
	src/client/vulkan/pipeline_cache.cpp
	src/client/vulkan/pipeline_layout.cpp
	src/client/vulkan/shader_module.cpp
	src/client/gfx_runtime_config.cpp
	src/client/gui.cpp
	src/client/render.cpp
	src/client/input_event_adapter.cpp
	src/common/assets/png_tools.cpp
	src/common/terrain/allocator.cpp
	src/common/terrain/cache.cpp
	src/common/terrain/chunk.cpp
	src/common/terrain/controller.cpp
	src/common/terrain/control_block.cpp
	src/common/terrain/coord.cpp
	src/common/terrain/generator.cpp
	src/common/terrain/hermite_data.cpp
	src/common/terrain/loader.cpp
	src/common/terrain/primary_data.cpp
	src/common/terrain/qef_solver.cpp
	src/common/terrain/surface.cpp
	src/common/terrain/surface_builder.cpp
	src/common/terrain/voxel_grid.cpp
	src/common/config.cpp
	src/common/filemanager.cpp
	src/common/gameview.cpp
	src/common/player.cpp
	src/common/runtime_config.cpp
	src/common/world_state.cpp
	src/server/world.cpp
	src/util/aabb.cpp
	src/util/allocator.cpp
	src/util/debug.cpp
	src/util/elapsed_timer.cpp
	src/util/error_condition.cpp
	src/util/exception.cpp
	src/util/hash.cpp
	src/util/log.cpp
	src/voxen/client/main_thread_service.cpp
	src/voxen/common/pipe_memory_allocator.cpp
	src/voxen/common/thread_pool.cpp
	src/voxen/common/uid.cpp
	src/voxen/debug/thread_name.cpp
	src/voxen/debug/uid_registry.cpp
	src/voxen/gfx/vk/frame_context.cpp
	src/voxen/gfx/vk/legacy_render_graph.cpp
	src/voxen/gfx/vk/render_graph.cpp
	src/voxen/gfx/vk/render_graph_builder.cpp
	src/voxen/gfx/vk/render_graph_execution.cpp
	src/voxen/gfx/vk/render_graph_private.cpp
	src/voxen/gfx/vk/render_graph_private.hpp
	src/voxen/gfx/vk/render_graph_resource.cpp
	src/voxen/gfx/vk/render_graph_runner.cpp
	src/voxen/gfx/vk/vk_debug_utils.cpp
	src/voxen/gfx/vk/vk_device.cpp
	src/voxen/gfx/vk/vk_device_wrappers.cpp
	src/voxen/gfx/vk/vk_error.cpp
	src/voxen/gfx/vk/vk_instance.cpp
	src/voxen/gfx/vk/vk_instance_wrappers.cpp
	src/voxen/gfx/vk/vk_physical_device.cpp
	src/voxen/gfx/vk/vk_private_consts.hpp
	src/voxen/gfx/vk/vk_swapchain.cpp
	src/voxen/gfx/vk/vk_utils.cpp
	src/voxen/land/chunk_ticket.cpp
	src/voxen/land/compressed_chunk_storage.cpp
	src/voxen/land/land_chunk.cpp
	src/voxen/land/land_private_consts.hpp
	src/voxen/land/land_private_messages.hpp
	src/voxen/land/land_service.cpp
	src/voxen/land/land_state.cpp
	src/voxen/land/pseudo_chunk_data.cpp
	src/voxen/land/pseudo_chunk_surface.cpp
	src/voxen/os/futex.cpp
	src/voxen/os/glfw_window.cpp
	src/voxen/os/process.cpp
	src/voxen/os/stdlib.cpp
	src/voxen/svc/engine.cpp
	src/voxen/svc/message_handling.cpp
	src/voxen/svc/message_queue.cpp
	src/voxen/svc/message_sender.cpp
	src/voxen/svc/messaging_private.cpp
	src/voxen/svc/messaging_private.hpp
	src/voxen/svc/messaging_service.cpp
	src/voxen/svc/service_base.cpp
	src/voxen/svc/service_locator.cpp
	src/voxen/util/concentric_octahedra_walker.cpp
	src/voxen/util/futex_work_counter.cpp
	src/voxen/util/packed_color.cpp
)

target_sources(game PRIVATE
	src/main.cpp
)

target_sources(voxen PRIVATE ${VOXEN_SOURCES})
source_group(TREE ${CMAKE_SOURCE_DIR}/src PREFIX Sources FILES ${VOXEN_SOURCES})

find_program(GLSL_COMPILER glslc)
if(${GLSL_COMPILER} STREQUAL "GLSL_COMPILER-NOTFOUND")
	message(FATAL_ERROR "GLSL compiler not found!")
endif()

set(VOXEN_SHADERS
	debug/octree.vert
	debug/octree.frag
	terrain/frustum_cull.comp
	terrain/simple.vert
	terrain/simple.frag
)

set(VOXEN_SHADER_COMPILER_OPTIONS
	-O --target-env=vulkan1.3 -Werror
	-I${CMAKE_SOURCE_DIR}/src/shaders/include
)

add_custom_target(voxen-shaders)
set_target_properties(voxen-shaders PROPERTIES FOLDER codegen)

foreach(SHADER ${VOXEN_SHADERS})
	# Input GLSL file
	set(SHADER_SOURCE ${CMAKE_SOURCE_DIR}/src/shaders/${SHADER})
	# Output SPIR-V file
	set(SHADER_BIN data/assets/shaders/${SHADER}.spv)
	# Output .d file to account for changed `#include`s
	set(SHADER_DEPFILE shader_depfiles/${SHADER}.d)

	add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/$<CONFIG>/${SHADER_BIN}
			${CMAKE_BINARY_DIR}/$<CONFIG>/${SHADER_DEPFILE}
		MAIN_DEPENDENCY ${SHADER_SOURCE}
		DEPFILE ${CMAKE_BINARY_DIR}/$<CONFIG>/${SHADER_DEPFILE}
		COMMENT "Building SPIR-V for shader ${SHADER}"
		COMMAND ${GLSL_COMPILER} ${VOXEN_SHADER_COMPILER_OPTIONS}
			# Enable debug information generation for debuggable builds
			$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:-g>
			${SHADER_SOURCE} -o ${CMAKE_BINARY_DIR}/$<CONFIG>/${SHADER_BIN}
			-MD -MF ${CMAKE_BINARY_DIR}/$<CONFIG>/${SHADER_DEPFILE} -MT $<CONFIG>/${SHADER_BIN}
	)

	target_sources(voxen-shaders PRIVATE ${CMAKE_BINARY_DIR}/$<CONFIG>/${SHADER_BIN})
endforeach()

add_dependencies(voxen voxen-shaders)

add_subdirectory(src/tools)
