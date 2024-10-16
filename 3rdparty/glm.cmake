voxen_add_library(glm INTERFACE)
add_library(3rdparty::glm ALIAS glm)

# GLM code triggers some compiler warnings.
# Mark as system headers to avoid failures in user code.
target_include_directories(glm SYSTEM INTERFACE glm)
