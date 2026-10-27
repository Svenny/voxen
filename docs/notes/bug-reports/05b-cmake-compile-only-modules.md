# C++ modules: BMIs of a `$<COMPILE_ONLY:lib>` dependency are not made available to the consumer

## Summary

A target that links a module-providing library through `$<COMPILE_ONLY:...>` cannot import its
modules: the library is not in `linked-target-dirs` of the consumer's `CXXDependInfo.json`, the
consumer's modmap is empty, and the compile fails with `fatal error: module 'm' not found`.
Plain `PRIVATE m` works. Module interfaces are needed at compile time only, so
`$<COMPILE_ONLY:...>` (documented as "Content of `...`, when collecting transitive compile
properties") looks like exactly the tool for "use the modules but don't link the library" (e.g. a
static library whose modules are also used by a DLL that must not link the library's code again).

## Environment

- CMake 4.2.3 + Ninja 1.13.2 + clang++ 21.1.8 (Linux, libstdc++ 15)
- CMake 4.4.4 + Ninja + clang-cl 21.1.8 (Windows 11)

## Files (`05b-cmake-compile-only-modules/`)

`CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.28)
project(compile_only_modules CXX)
set(CMAKE_CXX_STANDARD 20)

add_library(m STATIC)
target_sources(m PUBLIC FILE_SET CXX_MODULES FILES m.cppm)

add_executable(use use.cpp)
target_link_libraries(use PRIVATE $<COMPILE_ONLY:m>)
```

`m.cppm`:

```c++
export module m;
export inline int answer() { return 42; }
```

`use.cpp`:

```c++
import m;
int main() { return answer() == 42 ? 0 : 1; }
```

## Steps

```
CXX=clang++ cmake -S . -B build -G Ninja
cmake --build build
```

## Expected

`use` builds (the only function used is inline, so nothing from `m`'s object file is needed).

## Actual

```
use.cpp:1:8: fatal error: module 'm' not found
    1 | import m;
      | ~~~~~~~^
1 error generated.
ninja: build stopped: subcommand failed.
```

`build/CMakeFiles/use.dir/CXXDependInfo.json` has `"linked-target-dirs": []`; the modmap for
`use.cpp.obj` is empty. With `target_link_libraries(use PRIVATE m)` the same project builds and
runs.

## Verified

Linux, CMake 4.2.3, clang++ 21.1.8, Ninja: fails as shown; `PRIVATE m` control builds and exits 0.
Windows 11, CMake 4.4.4, Ninja, clang-cl 21.1.8: fails the same way (`fatal error: module 'm' not
found`). Not checked: 4.5.0-rc1, Makefiles/VS generators, GCC/MSVC.

No existing issue found on gitlab.kitware.com/cmake/cmake (searched COMPILE_ONLY,
linked-target-dirs; #27245 is about transitive deps per configuration, not this).
