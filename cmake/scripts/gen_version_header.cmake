# Generates `version.hpp` build-config file. Run in script mode:
#     cmake -D OUTPUT_FILE=... -D VERSION_MAJOR=... -D VERSION_MINOR=... -D VERSION_PATCH=...
#           [-D VERSION_SUFFIX=...] [-D GIT_EXECUTABLE=...] -P gen_version_header.cmake
#
# Runs on every build to catch new commits and local changes.
# `configure_file` doesn't touch the output when its contents stay the same,
# so dependent sources are not rebuilt unless the version actually changes.

set(GIT_HASH "unknown")

if(GIT_EXECUTABLE)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} describe --always --dirty --abbrev=16
		WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
		OUTPUT_VARIABLE GIT_DESCRIBE_OUTPUT
		OUTPUT_STRIP_TRAILING_WHITESPACE
		RESULT_VARIABLE GIT_DESCRIBE_RESULT
		ERROR_QUIET
	)

	# This can fail on source copies without Git history
	if(GIT_DESCRIBE_RESULT EQUAL 0)
		set(GIT_HASH "${GIT_DESCRIBE_OUTPUT}")
	endif()
endif()

set(VERSION_SUFFIX_DASHED "")
if(VERSION_SUFFIX)
	set(VERSION_SUFFIX_DASHED "-${VERSION_SUFFIX}")
endif()

configure_file(${CMAKE_CURRENT_LIST_DIR}/version.hpp.in ${OUTPUT_FILE} @ONLY)
