set(EXTRAS_HEADERS
	include/extras/attributes.hpp
	include/extras/bitset.hpp
	include/extras/defer.hpp
	include/extras/dyn_array.hpp
	include/extras/enum_utils.hpp
	include/extras/fixed_pool.hpp
	include/extras/function_ref.hpp
	include/extras/linear_allocator.hpp
	include/extras/pimpl.hpp
	include/extras/refcnt_ptr.hpp
)

target_include_directories(extras INTERFACE include)
target_sources(extras PRIVATE ${EXTRAS_HEADERS})
source_group(TREE ${CMAKE_SOURCE_DIR}/include/extras PREFIX Headers FILES ${EXTRAS_HEADERS})
# Also has visualizers for 3rdparty types, not to invent one more directory for them
voxen_target_natvis(extras include/extras/extras.natvis)

set(M_HEADERS
	include/m/color.hpp
	include/m/format.hpp
	include/m/fwd.hpp
	include/m/m.hpp
	include/m/matrix.hpp
	include/m/packing.hpp
	include/m/quaternion.hpp
	include/m/transform.hpp
	include/m/vector.hpp
)

target_include_directories(m INTERFACE include)
target_sources(m PRIVATE ${M_HEADERS})
source_group(TREE ${CMAKE_SOURCE_DIR}/include/m PREFIX Headers FILES ${M_HEADERS})
voxen_target_natvis(m include/m/m.natvis)

set(TK_HEADERS
	include/tk/cli.hpp
	include/tk/demangle.hpp
	include/tk/format.hpp
	include/tk/fs.hpp
	include/tk/hash.hpp
	include/tk/json.hpp
	include/tk/parse_error.hpp
	include/tk/png.hpp
	include/tk/rng.hpp
	include/tk/toml.hpp
	include/tk/uid.hpp
)

target_include_directories(tk PUBLIC include)
target_sources(tk PRIVATE ${TK_HEADERS})
source_group(TREE ${CMAKE_SOURCE_DIR}/include/tk PREFIX Headers FILES ${TK_HEADERS})

set(VOXEN_HEADERS
	include/voxen/assets/asset_base.hpp
	include/voxen/assets/asset_decoder.hpp
	include/voxen/assets/asset_service.hpp
	include/voxen/assets/assets_fwd.hpp
	include/voxen/client/gameview.hpp
	include/voxen/client/gui.hpp
	include/voxen/client/main_thread_service.hpp
	include/voxen/client/input_event_adapter.hpp
	include/voxen/client/player_action_events.hpp
	include/voxen/debug/bug_found.hpp
	include/voxen/debug/thread_name.hpp
	include/voxen/debug/uid_registry.hpp
	include/voxen/gfx/graph/land_render_passes.hpp
	include/voxen/gfx/graph/render_graph_common.hpp
	include/voxen/gfx/ui/ui_builder.hpp
	include/voxen/gfx/ui/ui_fwd.hpp
	include/voxen/gfx/ui/ui_render_data.hpp
	include/voxen/gfx/ui/ui_setup_types.hpp
	include/voxen/gfx/ui/ui_system.hpp
	include/voxen/gfx/vk/frame_context.hpp
	include/voxen/gfx/vk/legacy_render_graph.hpp
	include/voxen/gfx/vk/render_graph.hpp
	include/voxen/gfx/vk/render_graph_builder.hpp
	include/voxen/gfx/vk/render_graph_execution.hpp
	include/voxen/gfx/vk/render_graph_resource.hpp
	include/voxen/gfx/vk/render_graph_runner.hpp
	include/voxen/gfx/vk/standard_render_graph.hpp
	include/voxen/gfx/vk/vk_command_allocator.hpp
	include/voxen/gfx/vk/vk_debug_utils.hpp
	include/voxen/gfx/vk/vk_descriptor_set_layout.hpp
	include/voxen/gfx/vk/vk_device.hpp
	include/voxen/gfx/vk/vk_dma_system.hpp
	include/voxen/gfx/vk/vk_error.hpp
	include/voxen/gfx/vk/vk_include.hpp
	include/voxen/gfx/vk/vk_instance.hpp
	include/voxen/gfx/vk/vk_mesh_streamer.hpp
	include/voxen/gfx/vk/vk_physical_device.hpp
	include/voxen/gfx/vk/vk_pipeline.hpp
	include/voxen/gfx/vk/vk_pipeline_cache.hpp
	include/voxen/gfx/vk/vk_pipeline_layout.hpp
	include/voxen/gfx/vk/vk_swapchain.hpp
	include/voxen/gfx/vk/vk_transient_buffer_allocator.hpp
	include/voxen/gfx/vk/vk_utils.hpp
	include/voxen/gfx/vk/vma_fwd.hpp
	include/voxen/gfx/font_renderer.hpp
	include/voxen/gfx/frame_tick_id.hpp
	include/voxen/gfx/frame_tick_source.hpp
	include/voxen/gfx/gfx_land_loader.hpp
	include/voxen/gfx/gfx_options.hpp
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
	include/voxen/land/land_temp_blocks.hpp
	include/voxen/land/land_utils.hpp
	include/voxen/land/pseudo_chunk_data.hpp
	include/voxen/land/pseudo_chunk_surface.hpp
	include/voxen/os/dynamic_library.hpp
	include/voxen/os/file.hpp
	include/voxen/os/futex.hpp
	include/voxen/os/futex_work_counter.hpp
	include/voxen/os/os_fwd.hpp
	include/voxen/os/process.hpp
	include/voxen/os/time.hpp
	include/voxen/os/window.hpp
	include/voxen/os/window_events.hpp
	include/voxen/svc/async_file_io_service.hpp
	include/voxen/svc/config.hpp
	include/voxen/svc/engine.hpp
	include/voxen/svc/file_manager.hpp
	include/voxen/svc/message_handling.hpp
	include/voxen/svc/message_queue.hpp
	include/voxen/svc/message_sender.hpp
	include/voxen/svc/message_types.hpp
	include/voxen/svc/messaging_service.hpp
	include/voxen/svc/pipe_memory_allocator.hpp
	include/voxen/svc/pipe_memory_function.hpp
	include/voxen/svc/runtime_config.hpp
	include/voxen/svc/service_base.hpp
	include/voxen/svc/service_locator.hpp
	include/voxen/svc/svc_fwd.hpp
	include/voxen/svc/task_builder.hpp
	include/voxen/svc/task_context.hpp
	include/voxen/svc/task_coro.hpp
	include/voxen/svc/task_handle.hpp
	include/voxen/svc/task_service.hpp
	include/voxen/util/concentric_octahedra_walker.hpp
	include/voxen/util/lru_visit_ordering.hpp
	include/voxen/util/private_object_pool.hpp
	include/voxen/util/scratch_memory_allocator.hpp
	include/voxen/util/scratch_memory_containers.hpp
	include/voxen/util/shared_object_pool.hpp
	include/voxen/util/tick_id.hpp
	include/voxen/util/util_fwd.hpp
	include/voxen/util/v8g_concepts.hpp
	include/voxen/util/v8g_flat_map.hpp
	include/voxen/util/v8g_flat_map_impl.hpp
	include/voxen/util/v8g_hash_trie.hpp
	include/voxen/util/v8g_hash_trie_impl.hpp
	include/voxen/util/v8g_helpers.hpp
	include/voxen/world/player.hpp
	include/voxen/world/player_state_message.hpp
	include/voxen/world/world_control_service.hpp
	include/voxen/world/world_fwd.hpp
	include/voxen/world/world_state.hpp
	include/voxen/error_condition.hpp
	include/voxen/exception.hpp
	include/voxen/log.hpp
	include/voxen/uid.hpp
	include/voxen/visibility.hpp
)

target_include_directories(voxen PUBLIC include)
target_sources(voxen PRIVATE ${VOXEN_HEADERS})
source_group(TREE ${CMAKE_SOURCE_DIR}/include/voxen PREFIX Headers FILES ${VOXEN_HEADERS})
voxen_target_natvis(voxen include/voxen/svc/svc.natvis)

# Generate version header
include(VoxenCodegen)
voxen_add_version_header(voxen voxen/version.hpp)
