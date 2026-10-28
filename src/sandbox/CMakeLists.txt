# Research demos and experiments, built with `VOXEN_BUILD_SANDBOX=ON`

voxen_add_executable(demo-coroutines demo_coroutines.cpp)
target_link_libraries(demo-coroutines PRIVATE voxen tk)

voxen_add_executable(demo-stacktrace demo_stacktrace.cpp)
target_link_libraries(demo-stacktrace PRIVATE voxen)

# Micro-benchmark of V8G as a per-component store, prints a results table
# (see docs/notes/design-principles-draft.md, P2.6). Run from the Release preset.
# Linux only: allocation accounting and peak RSS reading are glibc/procfs-specific.
if(LINUX)
	voxen_add_executable(bench-v8g bench_v8g.cpp)
	target_link_libraries(bench-v8g PRIVATE extras tk 3rdparty::fmt)
endif()
