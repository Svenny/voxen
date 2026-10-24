# Research demos and experiments, built with `VOXEN_BUILD_SANDBOX=ON`

# Make all targets created here appear in a folder in VS
set(CMAKE_FOLDER sandbox)

voxen_add_executable(demo-coroutines demo_coroutines.cpp)
target_link_libraries(demo-coroutines PRIVATE voxen tk)

voxen_add_executable(demo-stacktrace demo_stacktrace.cpp)
target_link_libraries(demo-stacktrace PRIVATE voxen)
