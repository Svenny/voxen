# Make all test-related targets appear in a folder in VS
set(CMAKE_FOLDER tests)

voxen_add_library(test-common STATIC)

target_sources(test-common PRIVATE
	test_common.hpp
	test_common.cpp
	test_common_main.cpp
	voxen_test_common.hpp
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
