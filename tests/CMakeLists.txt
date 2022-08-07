voxen_add_executable(test-extras)

target_sources(test-extras PRIVATE
	extras/bitset.cpp
	extras/defer.cpp
	extras/fixed_pool.cpp
	extras/function_ref.cpp
	extras/linear_allocator.cpp
	extras/main.cpp
	extras/refcnt_ptr.cpp
	extras/source_location.cpp
)

target_link_libraries(test-extras PRIVATE
	extras
	3rdparty::catch2
	3rdparty::glm
)

add_test(NAME extras-bitset COMMAND test-extras "[extras::bitset]")
add_test(NAME extras-defer COMMAND test-extras "[extras::defer]")
add_test(NAME extras-fixed-pool COMMAND test-extras "[extras::fixed_pool]")
add_test(NAME extras-function-ref COMMAND test-extras "[extras::function_ref]")
add_test(NAME extras-linear-allocator COMMAND test-extras "[extras::linear_allocator]")
add_test(NAME extras-refcnt-ptr COMMAND test-extras "[extras::refcnt_ptr]")
add_test(NAME extras-source-location COMMAND test-extras "[extras::source_location]")
