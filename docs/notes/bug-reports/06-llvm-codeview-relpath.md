# [CodeView] `-fdebug-prefix-map` that yields a relative file name with an empty directory produces a drive-root path (`\src\main.cpp`); lld-link turns it into `C:\src\main.cpp`

## Summary

When a prefix map strips the source root (`-fdebug-prefix-map=C:/proj/=` or `=.`), the function's
`DIFile` gets `directory: ""` and a relative `filename`. `CodeViewDebug::getFullFilepath()` joins
them as `Dir + "\\" + Filename`, so the file checksum / line table name becomes `\src\main.cpp`, a
path relative to the root of the current drive rather than to the build directory. lld-link then
makes it absolute against the current drive (`C:\src\main.cpp`; with `/PDBSOURCEPATH:D:\build` it
gives `D:\build\src\main.cpp`). MSVC `link.exe` keeps `\src\main.cpp`. Debuggers resolving that path look in the drive root.

Expected (by analogy with the case without a prefix map, `clang-cl /c /Z7 src\main.cpp
/clang:-fdebug-compilation-dir=.`, which gives `.\src\main.cpp`): a relative name such as
`src\main.cpp` or `.\src\main.cpp`.

## Environment

Windows 11 x64, LLVM 21.1.8 (official installer: clang-cl, lld-link, llvm-readobj, llvm-pdbutil),
MSVC link.exe from VS 2026 Build Tools for comparison.

## Steps

`C:/proj/src/main.cpp` contains `int main() { return 0; }`. In `C:/proj`:

```
clang-cl /nologo /c /Z7 C:/proj/src/main.cpp /Foa.obj /clang:-fdebug-prefix-map=C:/proj/= /clang:-fdebug-compilation-dir=.
llvm-readobj --codeview a.obj | findstr Filename
lld-link /nologo /DEBUG a.obj /OUT:a.exe /PDB:a.pdb /SUBSYSTEM:CONSOLE /ENTRY:main /NODEFAULTLIB
llvm-pdbutil dump -modules -files a.pdb | findstr main.cpp
```

## Actual

```
Filename: \src\main.cpp (0x1)
- (MD5: 5C11FF1DEF313F2A87EABF3A30F130AD) C:\src\main.cpp
```

IR (`/clang:-S /clang:-emit-llvm`) shows where it comes from:

```
!1 = !DIFile(filename: "src\\main.cpp", directory: ".", ...)        ; CU
!12 = !DIFile(filename: "src/main.cpp", directory: "", ...)         ; subprogram
```

Results for a few variants (source passed as the absolute path `C:/proj/src/main.cpp`, as CMake
does):

| clang-cl flags | DIFile of `main` | CodeView name in .obj | PDB, lld-link | PDB, link.exe |
|---|---|---|---|---|
| `-fdebug-prefix-map=C:/proj/= -fdebug-compilation-dir=.` | `"src/main.cpp"`, dir `""` | `\src\main.cpp` | `C:\src\main.cpp` | `\src\main.cpp` |
| `-fdebug-prefix-map=C:/proj/=` | `"src/main.cpp"`, dir `""` | `\src\main.cpp` | `C:\src\main.cpp` | `\src\main.cpp` |
| `-fdebug-prefix-map=C:/proj=.` | `"./src/main.cpp"`, dir `""` | `\src\main.cpp` | `C:\src\main.cpp` | `\src\main.cpp` |
| `-fdebug-prefix-map=C:/proj/= -fdebug-compilation-dir=.`, lld-link `/PDBSOURCEPATH:D:\build` | same | `\src\main.cpp` | `D:\build\src\main.cpp` | |
| `-fdebug-prefix-map=C:/proj=X:/build` | `"src\\main.cpp"`, dir `"X:/build"` | `X:\build\src\main.cpp` | same | same |
| no prefix map, relative source `src\main.cpp`, `-fdebug-compilation-dir=.` | | `.\src\main.cpp` | `C:\proj\src\main.cpp` | `.\src\main.cpp` |

(All flags passed through `/clang:`.) The `=.` row also shows the `"\.\"` collapsing turning
`\.\src\main.cpp` into `\src\main.cpp`.

## Notes

- `getFullFilepath()` (llvm/lib/CodeGen/AsmPrinter/CodeViewDebug.cpp) has no case for an empty
  `Dir`; skipping the separator when `Dir` is empty would give `src\main.cpp`. Same function as
  #59878 (UNC paths), different problem.
- lld-link's absolutization of relative names is by design (D48882, discussion in #38851); this
  report is about the drive-root name the compiler emits.

## Verified

All rows of the table on Windows 11 with LLVM 21.1.8 and MSVC link.exe 14.51 (2026-10-25), in a
directory under `C:\Users\...` (paths shortened to `C:/proj` above). Not checked: LLVM trunk.

No existing issue found (searched llvm/llvm-project for CodeView/PDB with debug/file prefix map,
getFullFilepath, pdbsourcepath).
