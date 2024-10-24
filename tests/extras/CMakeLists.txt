voxen_add_executable(test-extras "")

target_sources(test-extras PRIVATE
	bitset.test.cpp
	defer.test.cpp
	dyn_array.test.cpp
	fixed_pool.test.cpp
	function_ref.test.cpp
	linear_allocator.test.cpp
	move_only_function.test.cpp
	refcnt_ptr.test.cpp
	source_location.test.cpp
)

target_link_libraries(test-extras PRIVATE
	test-common
	extras
)

add_test(NAME extras-bitset COMMAND test-extras "[extras::bitset]")
add_test(NAME extras-defer COMMAND test-extras "[extras::defer]")
add_test(NAME extras-dyn-array COMMAND test-extras "[extras::dyn_array]")
add_test(NAME extras-fixed-pool COMMAND test-extras "[extras::fixed_pool]")
add_test(NAME extras-function-ref COMMAND test-extras "[extras::function_ref]")
add_test(NAME extras-linear-allocator COMMAND test-extras "[extras::linear_allocator]")
add_test(NAME extras-move-only-function COMMAND test-extras "[extras::move_only_function]")
add_test(NAME extras-refcnt-ptr COMMAND test-extras "[extras::refcnt_ptr]")
add_test(NAME extras-source-location COMMAND test-extras "[extras::source_location]")
