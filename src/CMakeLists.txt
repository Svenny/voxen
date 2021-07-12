set(EXTRAS_SOURCES
	src/extras/spinlock.cpp
)

set(VOXEN_SOURCES
	src/client/vulkan/algo/debug_octree.cpp
	src/client/vulkan/algo/terrain_simple.cpp
	src/client/vulkan/high/main_loop.cpp
	src/client/vulkan/high/mesh.cpp
	src/client/vulkan/high/terrain_synchronizer.cpp
	src/client/vulkan/high/transfer_manager.cpp
	src/client/vulkan/backend.cpp
	src/client/vulkan/buffer.cpp
	src/client/vulkan/capabilities.cpp
	src/client/vulkan/command_buffer.cpp
	src/client/vulkan/command_pool.cpp
	src/client/vulkan/common.cpp
	src/client/vulkan/descriptor_manager.cpp
	src/client/vulkan/device.cpp
	src/client/vulkan/framebuffer.cpp
	src/client/vulkan/image.cpp
	src/client/vulkan/image_view.cpp
	src/client/vulkan/instance.cpp
	src/client/vulkan/memory.cpp
	src/client/vulkan/physical_device.cpp
	src/client/vulkan/pipeline.cpp
	src/client/vulkan/pipeline_cache.cpp
	src/client/vulkan/pipeline_layout.cpp
	src/client/vulkan/render_pass.cpp
	src/client/vulkan/shader_module.cpp
	src/client/vulkan/surface.cpp
	src/client/vulkan/swapchain.cpp
	src/client/vulkan/sync.cpp
	src/client/vulkan/texture_manager.cpp
	src/client/gui.cpp
	src/client/render.cpp
	src/client/window.cpp
	src/client/input_event_adapter.cpp
	src/common/terrain/allocator.cpp
	src/common/terrain/cache.cpp
	src/common/terrain/chunk.cpp
	src/common/terrain/chunk_id.cpp
	src/common/terrain/chunk_octree.cpp
	src/common/terrain/controller.cpp
	src/common/terrain/controller_seam_ops.cpp
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
	src/common/player.cpp
	src/common/threadpool.cpp
	src/common/world_state.cpp
	src/common/gameview.cpp
	src/common/config.cpp
	src/common/filemanager.cpp
	src/server/world.cpp
	src/util/aabb.cpp
	src/util/allocator.cpp
	src/util/debug.cpp
	src/util/elapsed_timer.cpp
	src/util/exception.cpp
	src/util/hash.cpp
	src/util/log.cpp
	src/main.cpp
)

target_sources(voxen PRIVATE ${VOXEN_SOURCES} ${EXTRAS_SOURCES})
source_group(TREE ${CMAKE_SOURCE_DIR}/src PREFIX Sources FILES ${VOXEN_SOURCES})
source_group(TREE ${CMAKE_SOURCE_DIR}/src/extras PREFIX Sources FILES ${EXTRAS_SOURCES})

find_program(GLSL_COMPILER glslc)
if(${GLSL_COMPILER} STREQUAL "GLSL_COMPILER-NOTFOUND")
	message(FATAL_ERROR "GLSL compiler not found!")
endif()

set(VOXEN_SHADERS
	debug/octree.vert
	debug/octree.frag
	terrain/simple.vert
	terrain/simple.frag
)

set(VOXEN_SHADER_BINARIES)

set(VOXEN_SHADER_COMPILER_OPTIONS -O --target-env=vulkan1.2 -Werror)
if (USE_VULKAN_DEBUGGING)
	# Enable debug information generation
	set(VOXEN_SHADER_COMPILER_OPTIONS ${VOXEN_SHADER_COMPILER_OPTIONS} -g)
endif()

foreach(SHADER ${VOXEN_SHADERS})
	set(SHADER_SOURCE ${CMAKE_SOURCE_DIR}/src/shaders/${SHADER})
	set(SHADER_BIN ${CMAKE_BINARY_DIR}/data/game/assets/shaders/${SHADER}.spv)
	get_filename_component(SHADER_DIR ${SHADER_BIN} DIRECTORY)
	file(MAKE_DIRECTORY ${SHADER_DIR})
	add_custom_command(OUTPUT ${SHADER_BIN} DEPENDS ${SHADER_SOURCE} COMMENT "Building SPIR-V for shader ${SHADER}"
		COMMAND ${GLSL_COMPILER} ${VOXEN_SHADER_COMPILER_OPTIONS} ${SHADER_SOURCE} -o ${SHADER_BIN})
	list(APPEND VOXEN_SHADER_BINARIES ${SHADER_BIN})
	message(STATUS "Compiling shader ${SHADER}, source ${SHADER_SOURCE}, binary ${SHADER_BIN}")
endforeach()

add_custom_target(voxen-shaders DEPENDS ${VOXEN_SHADER_BINARIES})
add_dependencies(voxen voxen-shaders)
