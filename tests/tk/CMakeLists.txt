voxen_add_executable(test-tk "")

target_sources(test-tk PRIVATE
	cli.test.cpp
	demangle.test.cpp
	fs.test.cpp
	hash.test.cpp
	json.test.cpp
	png.test.cpp
	rng.test.cpp
	toml.test.cpp
)

target_link_libraries(test-tk PRIVATE
	test-common
	tk
)

add_test(NAME tk-cli COMMAND test-tk "[tk::cli]")
add_test(NAME tk-demangle COMMAND test-tk "[tk::demangle]")
add_test(NAME tk-fs COMMAND test-tk "[tk::fs]")
add_test(NAME tk-hash COMMAND test-tk "[tk::hash]")
add_test(NAME tk-json COMMAND test-tk "[tk::json]")
add_test(NAME tk-png COMMAND test-tk "[tk::png]")
add_test(NAME tk-rng COMMAND test-tk "[tk::rng]")
add_test(NAME tk-toml COMMAND test-tk "[tk::toml]")
