set(EXTRAS_HEADERS
	include/extras/attributes.hpp
	include/extras/bitset.hpp
	include/extras/defer.hpp
	include/extras/dyn_array.hpp
	include/extras/enum_utils.hpp
	include/extras/fixed_pool.hpp
	include/extras/function_ref.hpp
	include/extras/hardware_params.hpp
	include/extras/linear_allocator.hpp
	include/extras/math.hpp
	include/extras/move_only_function.hpp
	include/extras/move_only_function_impl.hpp
	include/extras/pimpl.hpp
	include/extras/refcnt_ptr.hpp
	include/extras/source_location.hpp
	include/extras/string_utils.hpp

	# Visualizers for VS debugger.
	# External libs also go here - not the best place
	# but I don't want to invent one more directory
	# just to place 3rdparty nativs files there.
	include/extras/extras.natvis
	include/extras/glm.natvis
)

target_include_directories(extras INTERFACE include)
target_sources(extras PRIVATE ${EXTRAS_HEADERS})
source_group(TREE ${CMAKE_SOURCE_DIR}/include/extras PREFIX Headers FILES ${EXTRAS_HEADERS})

set(VOXEN_HEADERS
	include/voxen/client/vulkan/backend.hpp
	include/voxen/client/vulkan/descriptor_set_layout.hpp
	include/voxen/client/vulkan/pipeline.hpp
	include/voxen/client/vulkan/pipeline_cache.hpp
	include/voxen/client/vulkan/pipeline_layout.hpp
	include/voxen/client/vulkan/shader_module.hpp
	include/voxen/client/gfx_runtime_config.hpp
	include/voxen/client/gui.hpp
	include/voxen/client/main_thread_service.hpp
	include/voxen/client/render.hpp
	include/voxen/client/input_event_adapter.hpp
	include/voxen/client/player_action_events.hpp
	include/voxen/common/assets/png_tools.hpp
	include/voxen/common/terrain/allocator.hpp
	include/voxen/common/terrain/cache.hpp
	include/voxen/common/terrain/chunk.hpp
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
	include/voxen/common/config.hpp
	include/voxen/common/filemanager.hpp
	include/voxen/common/gameview.hpp
	include/voxen/common/pipe_memory_allocator.hpp
	include/voxen/common/player.hpp
	include/voxen/common/player_state_message.hpp
	include/voxen/common/private_object_pool.hpp
	include/voxen/common/runtime_config.hpp
	include/voxen/common/shared_object_pool.hpp
	include/voxen/common/uid.hpp
	include/voxen/common/v8g_concepts.hpp
	include/voxen/common/v8g_flat_map.hpp
	include/voxen/common/v8g_flat_map_impl.hpp
	include/voxen/common/v8g_hash_trie.hpp
	include/voxen/common/v8g_hash_trie_impl.hpp
	include/voxen/common/world_state.hpp
	include/voxen/common/world_tick_id.hpp
	include/voxen/debug/bug_found.hpp
	include/voxen/debug/thread_name.hpp
	include/voxen/debug/uid_registry.hpp
	include/voxen/gfx/vk/frame_context.hpp
	include/voxen/gfx/vk/legacy_render_graph.hpp
	include/voxen/gfx/vk/render_graph.hpp
	include/voxen/gfx/vk/render_graph_builder.hpp
	include/voxen/gfx/vk/render_graph_execution.hpp
	include/voxen/gfx/vk/render_graph_resource.hpp
	include/voxen/gfx/vk/render_graph_runner.hpp
	include/voxen/gfx/vk/vk_command_allocator.hpp
	include/voxen/gfx/vk/vk_debug_utils.hpp
	include/voxen/gfx/vk/vk_device.hpp
	include/voxen/gfx/vk/vk_dma_system.hpp
	include/voxen/gfx/vk/vk_error.hpp
	include/voxen/gfx/vk/vk_include.hpp
	include/voxen/gfx/vk/vk_instance.hpp
	include/voxen/gfx/vk/vk_mesh_streamer.hpp
	include/voxen/gfx/vk/vk_physical_device.hpp
	include/voxen/gfx/vk/vk_swapchain.hpp
	include/voxen/gfx/vk/vk_transient_buffer_allocator.hpp
	include/voxen/gfx/vk/vk_utils.hpp
	include/voxen/gfx/vk/vma_fwd.hpp
	include/voxen/gfx/font_renderer.hpp
	include/voxen/gfx/frame_tick_id.hpp
	include/voxen/gfx/gfx_land_loader.hpp
	include/voxen/gfx/gfx_public_consts.hpp
	include/voxen/gfx/gfx_system.hpp
	include/voxen/gfx/gfx_fwd.hpp
	include/voxen/land/chunk_key.hpp
	include/voxen/land/chunk_ticket.hpp
	include/voxen/land/compressed_chunk_storage.hpp
	include/voxen/land/cube_array.hpp
	include/voxen/land/land_chunk.hpp
	include/voxen/land/land_fwd.hpp
	include/voxen/land/land_generator.hpp
	include/voxen/land/land_messages.hpp
	include/voxen/land/land_public_consts.hpp
	include/voxen/land/land_service.hpp
	include/voxen/land/land_state.hpp
	include/voxen/land/land_storage_tree.hpp
	include/voxen/land/land_storage_tree_node_ptr.hpp
	include/voxen/land/land_temp_blocks.hpp
	include/voxen/land/land_utils.hpp
	include/voxen/land/pseudo_chunk_data.hpp
	include/voxen/land/pseudo_chunk_surface.hpp
	include/voxen/land/storage_tree_common.hpp
	include/voxen/land/storage_tree_utils.hpp
	include/voxen/land/typed_storage_tree.hpp
	include/voxen/os/file.hpp
	include/voxen/os/futex.hpp
	include/voxen/os/glfw_window.hpp
	include/voxen/os/os_fwd.hpp
	include/voxen/os/process.hpp
	include/voxen/os/stdlib.hpp
	include/voxen/os/time.hpp
	include/voxen/server/world.hpp
	include/voxen/svc/async_file_io_service.hpp
	include/voxen/svc/engine.hpp
	include/voxen/svc/message_handling.hpp
	include/voxen/svc/message_queue.hpp
	include/voxen/svc/message_sender.hpp
	include/voxen/svc/message_types.hpp
	include/voxen/svc/messaging_service.hpp
	include/voxen/svc/pipe_memory_function.hpp
	include/voxen/svc/service_base.hpp
	include/voxen/svc/service_locator.hpp
	include/voxen/svc/svc_fwd.hpp
	include/voxen/svc/task_builder.hpp
	include/voxen/svc/task_context.hpp
	include/voxen/svc/task_coro.hpp
	include/voxen/svc/task_handle.hpp
	include/voxen/svc/task_service.hpp
	include/voxen/util/aabb.hpp
	include/voxen/util/allocator.hpp
	include/voxen/util/concentric_octahedra_walker.hpp
	include/voxen/util/debug.hpp
	include/voxen/util/elapsed_timer.hpp
	include/voxen/util/error_condition.hpp
	include/voxen/util/exception.hpp
	include/voxen/util/futex_work_counter.hpp
	include/voxen/util/hash.hpp
	include/voxen/util/log.hpp
	include/voxen/util/lru_visit_ordering.hpp
	include/voxen/util/packed_color.hpp
	include/voxen/util/resolution.hpp
	include/voxen/util/tagged_tick_id.hpp
	include/voxen/visibility.hpp

	# Visualizers for VS debugger
	include/voxen/svc/svc.natvis
)

target_include_directories(voxen PUBLIC include)
target_sources(voxen PRIVATE ${VOXEN_HEADERS})
source_group(TREE ${CMAKE_SOURCE_DIR}/include/voxen PREFIX Headers FILES ${VOXEN_HEADERS})

# Generate version header
add_custom_target(voxen-version-file
	COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/cmake/gen-version-header.py
		${CMAKE_BINARY_DIR}/configs/voxen/version.hpp
		${VOXEN_VERSION_MAJOR} ${VOXEN_VERSION_MINOR} ${VOXEN_VERSION_PATCH} ${VOXEN_VERSION_SUFFIX}
	BYPRODUCTS ${CMAKE_BINARY_DIR}/configs/voxen/version.hpp
	COMMENT "Generating version.hpp file"
)
set_target_properties(voxen-version-file PROPERTIES FOLDER codegen)
add_dependencies(voxen voxen-version-file)

# Include directory with generated headers
target_include_directories(voxen PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/configs)
