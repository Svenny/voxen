set(GLFW_BUILD_DOCS OFF)
set(GLFW_INSTALL OFF)
set(GLFW_LIBRARY_TYPE STATIC)

add_subdirectory(glfw)
add_library(3rdparty::glfw ALIAS glfw)

# Add SYSTEM property, glfw headers trigger some compiler warnings
target_include_directories(glfw SYSTEM PUBLIC glfw/include)

voxen_fixup_3rdparty_target(glfw)
