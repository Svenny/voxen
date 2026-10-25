voxen_add_library(test-common STATIC)

target_sources(test-common PRIVATE
	test_common.hpp
	test_common.cpp
	voxen_test_common.hpp
)

target_link_libraries(test-common PUBLIC
	3rdparty::catch2-main
	3rdparty::fmt
	m
)

add_subdirectory(extras)
add_subdirectory(m)
add_subdirectory(tk)
add_subdirectory(voxen)
