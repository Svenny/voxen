voxen_add_library(test-common STATIC)

target_sources(test-common PRIVATE
	test_common.hpp
	test_common.cpp
	test_common_main.cpp
)

target_link_libraries(test-common PUBLIC
	3rdparty::catch2
	3rdparty::glm
)

target_link_libraries(test-common PRIVATE
	3rdparty::fmt
)

add_subdirectory(extras)
add_subdirectory(voxen)
