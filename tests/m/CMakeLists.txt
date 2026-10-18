voxen_add_executable(test-m "")

target_sources(test-m PRIVATE
	m_test_common.hpp
	matrix.test.cpp
	packing.test.cpp
	quaternion.test.cpp
	vector.test.cpp
)

target_link_libraries(test-m PRIVATE
	test-common
	m
)

add_test(NAME m-matrix COMMAND test-m "[m::matrix]")
add_test(NAME m-packing COMMAND test-m "[m::packing]")
add_test(NAME m-quaternion COMMAND test-m "[m::quaternion]")
add_test(NAME m-vector COMMAND test-m "[m::vector]")
