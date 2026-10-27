# Known bugs

Upstream bugs found while working on voxen, with report drafts, and known bugs of voxen itself.

## Upstream report drafts

Nothing has been filed or posted yet. Every report has its repro files in the directory of the same
name. SPIR-V binaries are not committed: assemble them with
`spirv-as --target-env vulkan1.3 X.spvasm -o X.spv`.

"winbox" = Windows 11 laptop with an AMD RX 6900 XT (AMD Software 26.10.1, amdvlk64.dll 32.0.21045)
plus Intel UHD, LLVM 21.1.8, MSVC 14.51, CMake 4.4.4. "Linux" = Ubuntu box, clang 21.1.8,
GCC 15.2, binutils 2.46, mold 2.40.4, CMake 4.2.3. Checked in 2026-10.

| # | Target tracker | Title | Repro verified | Existing upstream report |
|---|---|---|---|---|
| [1](01-amd-bda-pointers.md) | AMD Windows driver: [AMD Bug Report Tool](https://www.amd.com/en/resources/support-articles/faqs/AMDBRT.html); developer contact via the [AMD Developer Community Discord](https://discord.com/invite/amd-dev). The old community.amd.com forums redirect to amd.com; GPUOpen-Drivers/AMDVLK is the discontinued Linux driver | Pointer values: `OpPhi` with an `OpUndef` pointer operand crashes pipeline creation; storing a pointer to a storage buffer writes zero | Yes, winbox: crash / zeros on AMD, OK on Intel; controls OK. Also OK on RADV/NVIDIA/lavapipe (Linux). C repros `vk_pipe.c`, `vk_store.c` | None found |
| [2](02-amd-opname-nonsemantic.md) | Same as 1 | `vkCreateComputePipelines` access violation when `OpName` targets a NonSemantic `OpExtInst` (breaks Slang `-g2` shaders) | Yes, winbox: minimal hand-written module crashes on AMD, OK on Intel; 40+ variants bisected | [slang#8051](https://github.com/shader-slang/slang/issues/8051) (closed, same symptom, no root cause): draft comment in the report's appendix |
| [3](03-gcc-stacktrace-debuglink.md) | GCC bugzilla | (comment) `std::stacktrace` can't find the `.gnu_debuglink` file of the executable | Yes, Linux, GCC 15.2 (and a GCC 16 snapshot) | **Duplicate:** [GCC 123265](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=123265) (NEW, same root cause). Drafted an additional-data comment with strace and a suggested configure change |
| [4](04-gcc-demangle.md) | GCC bugzilla (demangler) | (comments) libiberty demangler fails on Clang `Tk`/`Tn` template args and on clone suffixes after data names | Yes, Linux: c++filt 2.46, `__cxa_demangle` of GCC 15.2 and 16 fail, llvm-cxxfilt 21 OK | **All three duplicates:** `Tk` [GCC 119025](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=119025), `Tn` [GCC 124295](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124295), data-name suffix (`.llvm.N`, also GCC's own `.lto_priv.N`) [GCC 59518](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=59518). Drafted a comment for each |
| [5](05-llvm-scandeps-pch.md) | [llvm-project](https://github.com/llvm/llvm-project/issues) (clang-scan-deps / clang:modules) | [clang-scan-deps] P1689 output has no `requires` for a clang-cl TU compiled with `/Yu` (MSVC-style PCH) | Yes, winbox, LLVM 21.1.8 | None found |
| [5b](05b-cmake-compile-only-modules.md) | [CMake](https://gitlab.kitware.com/cmake/cmake/-/issues) | C++ modules: BMIs of a `$<COMPILE_ONLY:lib>` dependency are not made available to the consumer | Yes, Linux (CMake 4.2.3, clang++) and winbox (CMake 4.4.4, clang-cl) | None found |
| [5c](05c-cmake-clangcl-tp-unused.md) | [CMake](https://gitlab.kitware.com/cmake/cmake/-/issues) | clang-cl + C++ modules: `-TP` together with `-x c++-module` from the modmap gives "argument unused during compilation: '-TP'" | Yes, winbox, CMake 4.4.4; root cause confirmed by hand (`-TP -x c++-module` warns, `/TP` alone doesn't) | None found (#18274 is a different cause) |
| [6](06-llvm-codeview-relpath.md) | [llvm-project](https://github.com/llvm/llvm-project/issues) (debuginfo, CodeView) | [CodeView] `-fdebug-prefix-map` that yields a relative file name with an empty directory produces a drive-root path (`\src\main.cpp`); lld-link turns it into `C:\src\main.cpp` | Yes, winbox, clang-cl/lld-link 21.1.8 and MSVC link.exe | None found (#59878 same function, different bug; #38851 is lld-link's absolutization by design) |
| [7](07-llvm-thinlto-suffix.md) | [llvm-project](https://github.com/llvm/llvm-project/issues) (LTO) | [ThinLTO] `.llvm.<hash>` suffixes of promoted locals depend on the absolute source path; `-ffile-prefix-map` does not make them reproducible | Yes, Linux, clang/lld 21.1.8 | None found |
| [8](08-mold-pt-note-alignment.md) | [mold](https://github.com/rui314/mold) | One PT_NOTE (p_align 8) holding 8-aligned and 4-aligned notes | Yes, Linux, mold 2.40.4 (readelf and llvm-readelf lose the build ID when reading by segment); GNU ld and lld split notes by alignment | **Covered by open PR** [mold#1740](https://github.com/rui314/mold/pull/1740). Short supporting comment drafted |
| [9](09-msstl-stacktrace-line.md) | [MS STL](https://github.com/microsoft/STL/issues) | `<stacktrace>`: `source_line()` / `source_file()` report the line after the call (return address not adjusted) | Yes, winbox, MSVC 14.51 (`/Od`, `/O2`) and clang-cl 21.1.8 | None found |

### Not drafted, and why

- **Intel UHD (101.7084):** access violation in `vkCreateGraphicsPipelines` on an optimized Slang
  vertex shader (`land/chunk_mesh.slang`) that loads 16-bit vector members (`int16_t2` at offset 0,
  `uint16_t4` at offset 4) of one `PhysicalStorageBuffer` struct. Reproduced standalone with the
  compiled shader, but a minimal GLSL equivalent does not crash, so not narrowed down yet. The
  attempts are in `intel-16bit-loads/`: `vk_gfxpipe.c` creates one graphics pipeline from a vertex
  and a fragment shader, `g_attr*.vert` are the GLSL variants that don't crash (paired with
  `triv.frag`; `triv.vert` is the trivial control), `bc_test.slang` + `vk_bc.c` (`vk_store.c` of
  report 1 with another input pattern) check the workaround's `bit_cast` unpacking in a compute
  shader. Next step: start from the compiled `chunk_mesh` SPIR-V and reduce it.
- **"Any shader with Shader.DebugInfo.100 crashes on AMD"**, as bug 2 was first described, is not
  accurate: glslang `-gV`/`-gVS` output (DebugInfo.100 and .102, with locals, parameters,
  `DebugDeclare`, `DebugValue`) works on the same driver. The crash needs an `OpName` on a
  non-semantic `OpExtInst`, which Slang emits and glslang doesn't. Report 2 is written that way.
- **Radeon GPU Analyzer** as the repro tool: not verified; the reports use small C programs instead.
- From the [modules experiment](../modules-experiment.md):
  - dllexport'ed variables from module BMIs ([llvm#87887](https://github.com/llvm/llvm-project/issues/87887))
    and missing strong module ownership in the MSVC ABI ([llvm#89781](https://github.com/llvm/llvm-project/issues/89781)):
    already reported.
  - `import std;` built with consumer warning flags (`-Wreserved-module-identifier`, `-Wundef` in
    `std.cc`): CMake #27601 and #27783, both closed for 4.5.0.
  - `#include` of std headers after `import std;` failing: known Clang issue (llvm#61329).
  - CMake refusing `CXX_MODULE_STD` for clang-cl + MS STL: a missing feature rather than a bug.
  - MSBuild/ClangCL toolset not supporting modules: Microsoft's props disable it deliberately.
  - Forward declarations before an `import` silently binding to the module entity: ill-formed code
    on our side; Clang not diagnosing it is not clearly a bug (and probably NDR).
  - `/NATVIS:` link option ignored by the VS ClangCL toolset: Microsoft/VS issue, not re-verified.

## Workarounds in voxen

Remove them once the drivers are fixed.

| Bug | Workaround | Where |
|---|---|---|
| AMD pointer stores (1) | Draw commands hold buffer addresses as `uint64_t`, not pointers | `src/shaders/land/mesh_layouts.slang` (`DrawCommand`) |
| AMD `OpName` on debug instructions (2) | Shader debug info defaults to `lines` (`-g1`, `OpLine` only); `-DVOXEN_SHADER_DEBUG_INFO=full` for RenderDoc source debugging on other GPUs. Alternative: strip `OpName`s of debug instructions after slangc, which would make `full` usable everywhere | `cmake/VoxenShaders.cmake` |
| Intel 16-bit member loads | Attributes are loaded as one `uint4` record and `bit_cast` | `src/shaders/land/chunk_mesh.slang` |
| libstdc++ debuglink (3) | `debug::bugFound` symbolizes stack traces through libstdc++'s internal `__glibcxx_backtrace_*` API on Linux; recheck at the next toolchain bump, GCC 123265 may remove the need | `src/voxen/debug/` |

## Known voxen bugs

- `world::ControlService`'s destructor auto-save runs after the services it uses are gone and
  doesn't wait for the simulation thread; `game_main` avoids triggering it.
- Continuing a frame after an exception isn't supported: the half-recorded command buffer isn't
  reset. Nothing tries to continue today.
- Debug builds wait ~9 s at teardown for in-flight land jobs.
