# clang-cl + C++ modules: `-TP` together with `-x c++-module` from the modmap gives "argument unused during compilation: '-TP'"

## Summary

For a module interface unit compiled with clang-cl, the Ninja generator puts `-TP` on the command
line and `-x c++-module` in the `@...modmap` response file. clang-cl lets the later `-x` win and
warns that `-TP` is unused (`-Wunused-command-line-argument`), which breaks `-Werror`/`/WX`
builds. Every `.cppm` in the project triggers it, and it can't be silenced per module unit
without per-source options; projects end up adding `-Wno-unused-command-line-argument` globally.

clang's behaviour looks reasonable here (`clang-cl /c -TP -x c++-module m.cppm` warns the same
way, while `clang-cl /c /TP m.cppm` alone does not warn), so this seems to need a CMake change:
don't emit `-TP` when the modmap supplies `-x c++-module`.

## Environment

Windows 11, CMake 4.4.4, Ninja, LLVM 21.1.8 clang-cl, VS 2026 Build Tools developer environment.

## Files (`05c-cmake-clangcl-tp-unused/`)

```cmake
cmake_minimum_required(VERSION 3.28)
project(tp_unused CXX)
set(CMAKE_CXX_STANDARD 20)

add_library(m STATIC)
target_sources(m PUBLIC FILE_SET CXX_MODULES FILES m.cppm)
target_compile_options(m PRIVATE -Werror)
```

```c++
// m.cppm
export module m;
export inline int answer() { return 42; }
```

## Steps

```
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang-cl
cmake --build build -- -v
```

## Expected

Builds without warnings.

## Actual

```
"C:\Program Files\LLVM\bin\clang-cl.exe"  /nologo -TP   /DWIN32 /D_WINDOWS /EHsc /Ob0 /Od /RTC1 -std:c++20 -MDd -Zi -Werror -clang:-MD -clang:-MTCMakeFiles\m.dir\m.cppm.obj -clang:-MFCMakeFiles\m.dir\m.cppm.obj.d @CMakeFiles\m.dir\m.cppm.obj.modmap /FoCMakeFiles\m.dir\m.cppm.obj /FdCMakeFiles\m.dir\m.pdb -c -- C:\...\m.cppm
clang-cl: error: argument unused during compilation: '-TP' [-Werror,-Wunused-command-line-argument]
```

`CMakeFiles/m.dir/m.cppm.obj.modmap`:

```
-x c++-module
-fmodule-output="CMakeFiles\m.dir\m.pcm"
```

Without `-Werror` it is the same message as a warning.

## Verified

Windows 11, CMake 4.4.4, clang-cl 21.1.8 (2026-10-25): the build above, plus by hand
`clang-cl /nologo /std:c++20 /c -TP -x c++-module m.cppm` (warns) vs. `clang-cl ... /c /TP m.cppm`
(no warning). Not checked: CMake 4.5.0-rc1.

No existing issue found on gitlab.kitware.com/cmake/cmake (#18274 is the same warning text but
for clang-tidy, a different cause).
