add_executable(test-extras
	extras/bitset.cpp
	extras/defer.cpp
	extras/main.cpp
	extras/source_location.cpp
)

target_include_directories(test-extras PUBLIC
	${CMAKE_SOURCE_DIR}/3rdparty/glm
	${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test-extras PUBLIC Catch2)

add_test(NAME extras-bitset COMMAND test-extras "[extras::bitset]")
add_test(NAME extras-defer COMMAND test-extras "[extras::defer]")
add_test(NAME extras-source-location COMMAND test-extras "[extras::source_location]")
