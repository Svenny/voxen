add_executable(test-extras
	extras/bitset.cpp
	extras/defer.cpp
	extras/main.cpp
)

target_include_directories(test-extras PUBLIC
	${CMAKE_SOURCE_DIR}/3rdparty/glm
	${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test-extras PUBLIC Catch2)
