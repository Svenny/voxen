# [ThinLTO] `.llvm.<hash>` suffixes of promoted locals depend on the absolute source path; `-ffile-prefix-map` does not make them reproducible

## Summary

Building the same sources in two different directories with `-ffile-prefix-map=$PWD/=` (and
`-fdebug-compilation-dir=.`) gives different `.llvm.<hash>` suffixes on promoted internal symbols,
so `.symtab`/`.strtab` (and therefore the build ID) differ, while everything else in the
executable is byte-identical. The bitcode objects differ only in `source_filename`, which keeps the
absolute path of the source file; the summary module hash and the GUIDs of internal symbols change
with it, and the suffix is derived from the module hash. Compiling with a relative source path
makes the outputs identical.

Build systems (CMake, Ninja-based ones in general) pass absolute source paths, so this blocks
reproducible ThinLTO builds across checkout directories.

## Environment

Ubuntu, clang 21.1.8 (Ubuntu 21.1.8-6ubuntu1), ld.lld 21.1.8, x86_64.

## Files (`07-llvm-thinlto-suffix/`)

```c++
// a.cpp
namespace { int counter; }
[[gnu::noinline]] static int bump() { return ++counter; }
int get() { return bump() + counter; }
```

```c++
// b.cpp
int get();
int main() { return get(); }
```

`build.sh <dir>` copies both files into `<dir>` and runs, inside `<dir>`:

```
F="-O2 -flto=thin -ffile-prefix-map=$PWD/= -fdebug-compilation-dir=."
clang++ $F -c "$PWD/a.cpp" -o a.o
clang++ $F -c "$PWD/b.cpp" -o b.o
clang++ -O2 -flto=thin -fuse-ld=lld a.o b.o -o prog
```

## Steps

```
./build.sh /tmp/x/dir1
./build.sh /tmp/x/other/dir2
nm /tmp/x/dir1/prog | grep llvm
nm /tmp/x/other/dir2/prog | grep llvm
```

## Expected

Identical `prog` in both directories.

## Actual

```
0000000000001730 t _ZL4bumpv.llvm.9628089822238950129
00000000000039dc b _ZN12_GLOBAL__N_17counterE.llvm.9628089822238950129
```

vs.

```
0000000000001730 t _ZL4bumpv.llvm.868714799969093745
00000000000039dc b _ZN12_GLOBAL__N_17counterE.llvm.868714799969093745
```

- Linked with `-Wl,--build-id=none` and then `strip --strip-all`, the two executables are
  identical; unstripped they differ only in the symbol names (and the build ID when enabled).
- `llvm-dis a.o` of the two builds differs only in `source_filename = "/tmp/x/dir1/a.cpp"` vs.
  `".../other/dir2/a.cpp"` and, in the summary, the module `hash:` and the GUIDs of the two
  internal symbols.
- Compiling as `clang++ -O2 -flto=thin -c a.cpp` (relative path, no prefix map) in two different
  directories gives identical executables, with the same suffix in both
  (`.llvm.7311954789195102804`).

## Possible fix

Apply the debug/file prefix map to the module's `source_filename` (it's what ends up in the
module hash and in local GUIDs), or derive the promotion suffix from something that doesn't
include the path.

## Verified

Everything above, locally on Linux (2026-10-25), clang/lld 21.1.8. Not checked: LLVM trunk,
other linkers (gold, mold with the LLVM plugin), Windows/lld-link.

No existing issue found (searched llvm/llvm-project issues for ThinLTO suffix/reproducible,
source_filename with prefix map, promoted locals).
