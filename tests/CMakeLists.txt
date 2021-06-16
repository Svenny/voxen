add_executable(test-extras
	${CMAKE_SOURCE_DIR}/src/extras/spinlock.cpp

	extras/bitset.cpp
	extras/defer.cpp
	extras/fixed_pool.cpp
	extras/function_ref.cpp
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
add_test(NAME extras-fixed-pool COMMAND test-extras "[extras::fixed_pool]")
add_test(NAME extras-function-ref COMMAND test-extras "[extras::function_ref]")
add_test(NAME extras-source-location COMMAND test-extras "[extras::source_location]")
