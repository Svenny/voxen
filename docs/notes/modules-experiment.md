# C++ modules experiment

Report of the 2026-10 experiment, the watch list, and the roadmap for the next attempt.

**Status: deferred.** Modules work on Ninja on both OSes, but cost more than they give until
`import std;` replaces textual std includes and the PCH, and the Windows DLL story improves.
Revisit with the next toolchain bump (Clang, CMake), starting from the [watch list](#watch-list).
The branch `svenny/cxx-modules-experiment` holds the converted code (last rebased onto master on
2026-10-25).

Toolchains: Clang 21.1.8 everywhere; Linux: libstdc++ 15, CMake 4.2.3 locally and 4.4.4 in CI,
Ninja 1.13; Windows: clang-cl 21.1.8 with MS STL 14.51, CMake 4.4.4.

## What was converted

- `m` (header-only math): **wrapper module**. `include/m/m.cppm` includes `<m/m.hpp>` in the global
  module fragment (GMF) and re-exports every name with `export namespace m { using m::...; }`
  (concepts, class templates, aliases, functions, operators: ~145 names). Entities stay attached to
  the global module, so `#include <m/...>` and `import m;` can be mixed freely, even in one TU. `m`
  became a STATIC library (an INTERFACE library can't own a module unit; the object holds the
  module initializer).
- `tk` (static library): **real modules**, headers deleted. Separate modules `tk.parse_error`,
  `tk.cli`, `tk.json` and `tk.toml` (both `export import tk.parse_error`), `tk.rng` (templates +
  `assert` + FP pragmas), `tk.fs`, `tk.png`, `tk.demangle`, umbrella `tk`. `tk.toml` has an
  interface partition `tk.toml:datetime`. Private module `tk.text_utils` (PRIVATE `CXX_MODULES` file
  set) is imported only by implementation units. Consumers: libvoxen TUs and public headers
  (`import tk.cli;` in a header), tests, host tools.
- Dummy libraries in `tests/modules`: `modshared` (SHARED, exported class with virtuals, functions
  taking `m` types, imports `m` and `tk.rng`), `modstatic` (STATIC, `export import modshared;`,
  derives from the exported class, definitions in the interface unit), `test-modules` (plain TU
  mixing `#include` of std/fmt/Vulkan/`m` headers with `import m; import modstatic; import tk;`),
  `modstd` + `test-import-std` (`import std;`, behind `-DVOXEN_IMPORT_STD=ON`).

## What works

| | Linux Ninja | Windows Ninja Multi-Config |
|---|---|---|
| `m` wrapper module | yes | yes |
| `tk` modules, partitions, private module | yes | yes |
| Module from a shared library, consumed by an executable | yes | yes, except exported **variables** (link error) |
| Static library module re-exporting a shared library module | yes | yes |
| `#include` + `import` mixed in one TU | yes (include first) | yes (include first) |
| PCH + `import` in one TU | yes | **no**, PCH must be disabled |
| `import std;` | yes (CMake native, libstdc++ 15) | yes, but only with a hand-made `std.ixx` target |
| Debug info / natvis | yes (gdb sees module types) | yes (PDB/natvis unchanged) |
| clangd | yes (with or without `--experimental-modules-support`) | not checked |
| Tests (`-Werror`, zero warnings) | all pass, Debug, also without PCH | all pass, Debug and Release |

The Visual Studio (MSBuild) generator could not build modules with the ClangCL toolset at all: CMake
delegates scanning to MSBuild's native module support, and VS's own `Microsoft.Cpp.ClangCl.*.props`
disable it ("clang-cl does not supports C++ modules yet"). That was one reason to drop the VS
generator, which is done.

CI runs of the branch: [39513752133](https://github.com/Svenny/voxen/actions/runs/39513752133) and,
after the rebase, [40145652502](https://github.com/Svenny/voxen/actions/runs/40145652502).

## Limitations, pitfalls and workarounds

1. **Forward declarations of module-attached entities silently break (Linux: link errors).**
   libvoxen headers forward-declare `namespace tk::cli { class Args; class Parser; }`. Those are
   declarations in the global module, a *different* entity from `tk::cli::Args@tk.cli`. Clang only
   diagnoses it when the forward declaration comes *after* the import. Before the import it accepts
   it and resolves to the module entity, so TUs that import and TUs that don't disagree:
   `undefined symbol: voxen::RuntimeConfig::fill(tk::cli::Args@tk.cli const&)`. On Windows names
   carry no module ownership (see 8), so it would even link. Fix: no forward declarations across a
   module boundary (import the module in the header), or keep such types in the global module with
   `export extern "C++" { ... }`.
2. **GMF declarations are not visible to importers**, not even to the module's own implementation
   units. Every unit has to include what it uses; even operators are hidden (`invalid operands to
   binary expression ('const std::vector<Value>' ...)` until `<vector>` was included). The
   include-what-you-use pass on master fixed the known gaps; more show up without PCH, with MS STL
   more than with libstdc++.
3. **Module dependencies must be real link dependencies.** Once a public libvoxen header imports
   `tk.cli`, consumers of libvoxen need its BMI. `$<COMPILE_ONLY:tk>` would be the right tool, but
   CMake ignores it for module lookup (`CXXDependInfo.json` lists only `linked-target-dirs`). `tk`
   is a PUBLIC dependency of libvoxen on master now anyway.
4. **Windows: PCH breaks module scanning.** CMake uses MSVC-style PCH (`/Yu /Fp /FI`) with clang-cl
   and passes those flags to `clang-scan-deps`, which then silently reports **no imports** for the
   TU (same command without `/Yu` finds them), and the compile fails with `module 'tk.toml' not
   found`. Linux (clang-style `-include-pch`) is fine. Workaround: no PCH on Windows (clean Debug
   44 s → 50 s). Real fixes: CMake using clang-style PCH for clang-cl, a clang-scan-deps fix
   ([bug report 5](bug-reports/05-llvm-scandeps-pch.md)), or replacing PCH with `import std;`.
5. **Windows: `dllexport` in a BMI stays `dllexport` in importers.** Functions still link through
   import library thunks (one extra jump per call), **exported variables don't link**
   ([llvm#87887](https://github.com/llvm/llvm-project/issues/87887)). Workaround: no exported data
   from DLL modules, use accessor functions. The export macro still comes from a header in the GMF;
   there's no per-consumer `dllimport` with modules.
6. **Windows: `dllexport` classes instantiate all special members**, so a class with a user-provided
   destructor triggers `-Wdeprecated-copy-with-user-provided-dtor` on Windows only. Declare copy
   operations explicitly.
7. **Windows: clang-cl warns `argument unused during compilation: '-TP'`** for `.cppm` units
   ([bug report 5c](bug-reports/05c-cmake-clangcl-tp-unused.md)), an error with `-Werror`. Needs
   `-Wno-unused-command-line-argument` (no way to target module units only in the toolchain setup;
   a per-source `COMPILE_OPTIONS` helper would be more precise).
8. **MSVC ABI has no strong module ownership in Clang**
   ([llvm#89781](https://github.com/llvm/llvm-project/issues/89781)): Windows mangling ignores
   attachment, Linux has `modshared::Counter@modshared::add`. Same code, different ODR failure
   modes per OS.
9. **Non-inline by default.** Member functions defined in a class body in a module purview are not
   implicitly `inline`, and Clang does not import bodies of non-inline functions, so without LTO
   importers can't inline them. Hot code (`m`) should stay a wrapper module (entities in the GMF).
10. **Macros are not exported.** They stay in headers included by importers: each library needs a
    small macros-only header (`voxen/visibility.hpp` style; log macros, `extras` attributes).
    `assert` inside module templates is expanded when the BMI is built: the module's `NDEBUG` wins.
11. **BMI flag compatibility.** Clang refuses BMIs built with different language options (`errno in
    math functions was enabled in precompiled file ... std.pcm`). CMake 4.2 builds `std` with global
    flags only, so `-fno-math-errno` etc. had to go to `CMAKE_CXX_FLAGS`. CMake 4.4 builds `std` per
    consumer with the consumer's flags, which drags our warnings into libstdc++'s `std.cc`: needs
    `-Wno-reserved-module-identifier -Wno-undef` on `import std` users (CMake #27601 and #27783,
    fixed for 4.5).
12. **`import std;` must come after `#include`s of std-using headers** (fmt, Vulkan, Catch2); the
    other order fails with libstdc++ ([llvm#61329](https://github.com/llvm/llvm-project/issues/61329)).
13. **`import std;` on Windows:** CMake refuses `CXX_MODULE_STD` for clang-cl + MS STL ("Only libc++
    and libstdc++ are supported"). clang-cl builds MS STL's `std.ixx` fine (with
    `-Wno-include-angled-in-module-purview`), so the branch adds a `voxen-std` static library with
    `$ENV{VCToolsInstallDir}/modules/std.ixx` as a module unit. Needs the VS developer environment
    at configure time.
14. **CMake's `import std` gate UUID changes per CMake version** (4.2: `d0edc3af-...`, 4.3:
    `451f2fe2-...`, 4.4: `f35a9ac6-...`), so `CMakeLists.txt` switches on `CMAKE_VERSION`.
15. **Scanning cost on every header change.** Touching a header rescans every TU including it, and
    scanning doesn't use the PCH. No cutoff for an unchanged BMI either (see below).
16. **Defining a GMF-declared entity in the module purview is an error.** `src/tk/demangle.cpp`
    defines LLVM demangler's out-of-line `itanium_demangle::parse_discriminator`; in the purview it
    gets attached to `tk.demangle`. Fix: `extern "C++"` on the definition. Any "define a hook
    declared by a third-party header" pattern needs the same.

### PCH coexistence (Linux)

The `voxen` PCH includes `m/m.hpp`; TUs got the PCH plus `import m;`, `import tk.toml;` or
`import tk.cli;` from public headers. All fine with clang-style PCH. On Windows see 4.

### Incremental rebuilds (Linux, Debug)

Touch (mtime only) and rebuild; all results correct:

| Touched | Scans | Compiles | Links | Rebuilt compiles |
|---|---|---|---|---|
| `tk/toml.cppm` (interface) | 1 | 7 | 10 | the unit, `tk.cppm`, `toml.cpp`, 4 importers |
| `tk/toml-datetime.cppm` (partition) | 1 | 8 | 10 | as above + `toml.cppm` |
| `src/tk/toml.cpp` (implementation) | 1 | 1 | 10 | only itself |
| `tk/cli.cppm` (imported by voxen headers) | 1 | 15 | 10 | every TU including those headers |
| `m/m.cppm` (wrapper) | 1 | 11 | 13 | importers of `m` only |
| `m/packing.hpp` (in `m.cppm` GMF and textual users) | 98 | 108 | 13 | everything including it, `m.cppm`, then importers |

Touching an interface unit always rebuilds importers. Correctness check: changing an inline function
in the `tk.toml` interface made its tests fail after an incremental build, reverting fixed them.

### Debug info, natvis, clangd

- gdb resolves module types from importers (`ptype tk::toml::Table`); module-attached functions show
  as `modstatic::sumOfSquares@modstatic(int, int)`.
- natvis works unchanged: the `m` wrapper keeps the type names, and module-attached types have the
  same names in CodeView (no ownership).
- clangd 21 without flags reuses CMake's BMIs via `compile_commands.json`: works only while those
  BMIs are built, up to date and from the same Clang version. With `--experimental-modules-support`
  it builds its own BMIs (13.5 s for one TU's prerequisites the first time) and works either way.

### Build time

Linux, clean Debug, own code only, 24 threads (CPU time = user+sys of the whole build):

| Variant | Wall | CPU | Steps |
|---|---|---|---|
| Baseline (headers + PCH) | 8.0..12 s | 159..169 s | 149 compiles |
| Modules + PCH | 8.9..10.1 s | 184..195 s | 161 scans + 162 compiles |
| Modules, no PCH | 11.8..12.3 s | 242..249 s | 161 scans + 161 compiles |

About +15% CPU with only `m` and `tk` converted: scanning is a full preprocessor pass per TU, and
module units' GMFs reparse std headers while most TUs still include the same std/fmt headers.
Windows clean Debug of the whole tree: baseline 44 s (PCH) / 50 s (no PCH), experiment 66 s (no
PCH, plus the `std` module and dummy libraries). Real gains need `import std;` (or a std/fmt module)
replacing the textual includes everywhere, i.e. replacing the PCH.

## Roadmap for the next attempt

Done on master: the VS generator is dropped, the include-what-you-use pass is done.

1. **Macro headers:** split macros out of headers that will become modules (log macros, `extras`
   attributes; `voxen/visibility.hpp` already is one).
2. **Remove cross-library forward declarations** of future module types: `tk::cli` is still
   forward-declared in `svc/engine.hpp`, `svc/runtime_config.hpp` and `gfx/gfx_options.hpp`.
3. **Enable scanning** for own targets (`CXX_SCAN_FOR_MODULES ON`, OFF for 3rdparty), the `-TP`
   workaround for clang-cl, no PCH on Windows (or wait for a scan-deps/CMake fix).
4. **Wrapper modules for header-only libraries** (`m`, `extras`): low risk, mixable with includes,
   no inline semantics change, natvis keeps working. `m.format` can be a second wrapper module
   importing fmt headers in its GMF.
5. **Leaf static libraries as real modules** (`tk`), one module per sub-namespace, partitions where
   a module is large, private modules for internals, separate implementation units (implementation
   edits rebuild only themselves).
6. **libvoxen last:** only after the DLL story improves (no exported variables), or keep headers + a
   wrapper module for its public API. Internal code could use attached modules first (not exported
   from the DLL).
7. **`import std;` everywhere,** once CMake supports MS STL with clang-cl (or keep the hand-made
   `std.ixx` target), then retire the PCH. Watch BMI flag compatibility and the include-before-import
   rule.

## Watch list

- [llvm#87887](https://github.com/llvm/llvm-project/issues/87887): dllexport variables from module BMIs.
- [llvm#89781](https://github.com/llvm/llvm-project/issues/89781): strong module ownership in the MSVC ABI.
- clang-scan-deps with `/Yu` ([bug report 5](bug-reports/05-llvm-scandeps-pch.md), not filed yet).
- CMake `$<COMPILE_ONLY>` module lookup ([bug report 5b](bug-reports/05b-cmake-compile-only-modules.md), not filed yet).
- CMake `import std` for clang-cl + MS STL.
- The CMake `import std` gate UUID changing per CMake version.
