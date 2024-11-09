voxen_add_executable(test-voxen "")

target_sources(test-voxen PRIVATE
	common/v8g_flat_map.test.cpp
	common/v8g_hash_trie.test.cpp
	debug/uid_registry.test.cpp
	land/chunk_key.test.cpp
	land/compressed_chunk_storage.test.cpp
	land/cube_array.test.cpp
	svc/message_queue.test.cpp
	svc/service_locator.test.cpp
	util/concentric_octahedra_walker.test.cpp
	util/hash.test.cpp
	util/packed_color.test.cpp
)

target_link_libraries(test-voxen PRIVATE
	test-common
	voxen
)

add_test(NAME voxen-v8g-flat-map COMMAND test-voxen "[voxen::v8g_flat_map]")
add_test(NAME voxen-v8g-hash-trie COMMAND test-voxen "[voxen::v8g_hash_trie]")
add_test(NAME voxen-debug-uid-registry COMMAND test-voxen "[voxen::debug::uid_registry]")
add_test(NAME voxen-land-chunk-key COMMAND test-voxen "[voxen::land::chunk_key]")
add_test(NAME voxen-compressed-chunk-storage COMMAND test-voxen "[voxen::land::compressed_chunk_storage]")
add_test(NAME voxen-land-cube-array COMMAND test-voxen "[voxen::land::cube_array]")
add_test(NAME voxen-svc-message-queue COMMAND test-voxen "[voxen::svc::message_queue]")
add_test(NAME voxen-svc-service-locator COMMAND test-voxen "[voxen::svc::service_locator]")
add_test(NAME voxen-concentric-octahedra-walker COMMAND test-voxen "[voxen::concentric_octahedra_walker]")
add_test(NAME voxen-hash COMMAND test-voxen "[voxen::hash]")
add_test(NAME voxen-packed-color COMMAND test-voxen "[voxen::packed_color]")
