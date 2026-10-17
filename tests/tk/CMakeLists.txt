voxen_add_executable(test-tk "")

target_sources(test-tk PRIVATE
	rng.test.cpp
	toml.test.cpp
)

target_link_libraries(test-tk PRIVATE
	test-common
	tk
)

add_test(NAME tk-rng COMMAND test-tk "[tk::rng]")
add_test(NAME tk-toml COMMAND test-tk "[tk::toml]")
