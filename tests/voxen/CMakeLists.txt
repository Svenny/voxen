voxen_add_executable(test-voxen "")

target_sources(test-voxen PRIVATE
	common/config.test.cpp
	common/private_object_pool.test.cpp
	common/scratch_memory_allocator.test.cpp
	common/shared_object_pool.test.cpp
	common/v8g_flat_map.test.cpp
	common/v8g_hash_trie.test.cpp
	debug/uid_registry.test.cpp
	land/chunk_key.test.cpp
	land/compressed_chunk_storage.test.cpp
	land/cube_array.test.cpp
	os/file.test.cpp
	svc/async_file_io_service.test.cpp
	svc/message_queue.test.cpp
	svc/service_locator.test.cpp
	svc/task_service.test.cpp
	util/concentric_octahedra_walker.test.cpp
	voxen_test_common.cpp
)

target_link_libraries(test-voxen PRIVATE
	test-common
	tk
	voxen
)

add_test(NAME voxen-config COMMAND test-voxen "[voxen::config]")
add_test(NAME voxen-private-object-pool COMMAND test-voxen "[voxen::private_object_pool]")
add_test(NAME voxen-scratch-memory-allocator COMMAND test-voxen "[voxen::scratch_memory_allocator]")
add_test(NAME voxen-shared-object-pool COMMAND test-voxen "[voxen::shared_object_pool]")
add_test(NAME voxen-v8g-flat-map COMMAND test-voxen "[voxen::v8g_flat_map]")
add_test(NAME voxen-v8g-hash-trie COMMAND test-voxen "[voxen::v8g_hash_trie]")
add_test(NAME voxen-debug-uid-registry COMMAND test-voxen "[voxen::debug::uid_registry]")
add_test(NAME voxen-land-chunk-key COMMAND test-voxen "[voxen::land::chunk_key]")
add_test(NAME voxen-compressed-chunk-storage COMMAND test-voxen "[voxen::land::compressed_chunk_storage]")
add_test(NAME voxen-land-cube-array COMMAND test-voxen "[voxen::land::cube_array]")
add_test(NAME voxen-file COMMAND test-voxen "[voxen::os::file]")
add_test(NAME voxen-svc-async-file-io-service COMMAND test-voxen "[voxen::svc::async_file_io_service]")
add_test(NAME voxen-svc-message-queue COMMAND test-voxen "[voxen::svc::message_queue]")
add_test(NAME voxen-svc-service-locator COMMAND test-voxen "[voxen::svc::service_locator]")
add_test(NAME voxen-svc-task-service COMMAND test-voxen "[voxen::svc::task_service]")
add_test(NAME voxen-concentric-octahedra-walker COMMAND test-voxen "[voxen::concentric_octahedra_walker]")
