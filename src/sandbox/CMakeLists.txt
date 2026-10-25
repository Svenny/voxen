# Research demos and experiments, built with `VOXEN_BUILD_SANDBOX=ON`

voxen_add_executable(demo-coroutines demo_coroutines.cpp)
target_link_libraries(demo-coroutines PRIVATE voxen tk)

voxen_add_executable(demo-stacktrace demo_stacktrace.cpp)
target_link_libraries(demo-stacktrace PRIVATE voxen)
