voxen_add_executable(test-voxen test_voxen_main.cpp)

target_sources(test-voxen PRIVATE
	common/land/cube_array.test.cpp
	common/v8g_flat_map.test.cpp
)

target_link_libraries(test-voxen PRIVATE
	3rdparty::catch2
	voxen
)

add_test(NAME voxen-land-cube-array COMMAND test-voxen "[voxen::land::cube_array]")
add_test(NAME voxen-v8g-flat-map COMMAND test-voxen "[voxen::v8g_flat_map]")
