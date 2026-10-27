# [clang-scan-deps] P1689 output has no `requires` for a clang-cl TU compiled with `/Yu` (MSVC-style PCH)

## Summary

With a clang-cl command line that uses a precompiled header (`/Yu<hdr> /Fp<pch>`),
`clang-scan-deps -format=p1689` reports no module imports for the TU. The same command without
`/Yu` reports them. The compile itself (same flags plus the BMI) works, so the scan result is
just wrong. CMake's Ninja generator passes the target's PCH flags to `clang-scan-deps`, so any
target that combines `target_precompile_headers` with `import` fails with
`fatal error: module '...' not found` under clang-cl.

## Environment

- Windows 11 x64, LLVM 21.1.8 (official installer: `clang-cl`, `clang-scan-deps`), MSVC 14.5x
  headers/libs from VS 2026 Build Tools (developer environment not required for this repro)

## Files (`05-llvm-scandeps-pch/`)

```c++
// pch.h
inline int from_pch() { return 1; }
// pch.cpp
// empty, only used to create the PCH
// a.cppm
export module a;
export int from_module() { return 2; }
// use.cpp
import a;
int main() { return from_pch() + from_module(); }
// use2.cpp
#include "pch.h"
import a;
int main() { return from_pch() + from_module(); }
```

## Steps

```
clang-cl /nologo /std:c++20 /c /Ycpch.h /Fppch.pch /FIpch.h pch.cpp /Fopch.obj

clang-scan-deps -format=p1689 -- clang-cl /nologo /std:c++20 /c /FIpch.h use.cpp /Fouse.obj
clang-scan-deps -format=p1689 -- clang-cl /nologo /std:c++20 /c /Yupch.h /Fppch.pch /FIpch.h use.cpp /Fouse.obj
```

## Expected

Both scans report `"requires": [{"logical-name": "a"}]`.

## Actual

Without `/Yu`:

```json
{
  "revision": 0,
  "rules": [
    {
      "primary-output": "use.obj",
      "requires": [
        {
          "logical-name": "a"
        }
      ]
    }
  ],
  "version": 1
}
```

With `/Yupch.h /Fppch.pch`:

```json
{
  "revision": 0,
  "rules": [
    {
      "primary-output": "use.obj"
    }
  ],
  "version": 1
}
```

Exit code 0, no diagnostics. Same result for `use2.cpp` (`#include "pch.h"` in the source
instead of `/FI`, scanned with `/Yupch.h /Fppch.pch`): `requires` present without `/Yu`, missing
with it.

The actual compile with the PCH works once the BMI is supplied:

```
clang-cl /nologo /std:c++20 /c a.cppm /Foa.obj /clang:-fmodule-output=a.pcm
clang-cl /nologo /std:c++20 /c /Yupch.h /Fppch.pch /FIpch.h use.cpp /Fouse.obj /clang:-fmodule-file=a=a.pcm
```

(both exit 0).

## Verified

All of the above on Windows 11 with LLVM 21.1.8 (2026-10-25). Originally found with CMake 4.4.4
(Ninja Multi-Config, clang-cl, `target_precompile_headers` + `import`), where the scan rule gets
`/Yu"cmake_pch.hxx" /Fp... /FI"cmake_pch.hxx"`; the resulting compile fails with
`fatal error: module '...' not found`. On Linux, clang-style PCH (`-include-pch`) scans fine in
the same CMake project. Not checked: LLVM trunk, `-format=experimental-full`.

No existing issue found (searched llvm/llvm-project issues for clang-scan-deps with PCH, `/Yu`,
precompiled header, P1689).
