voxen_add_library(simpleini STATIC)
add_library(3rdparty::simpleini ALIAS simpleini)

# Interface header of this library violates -Wsign-conversion and -Wundef
# Mark it as system header to avoid failures in user code
target_include_directories(simpleini SYSTEM INTERFACE simpleini)
target_link_libraries(simpleini PRIVATE dummy_suppress_3rdparty_warnings)
target_sources(simpleini PRIVATE simpleini/simpleini/ConvertUTF.c)
