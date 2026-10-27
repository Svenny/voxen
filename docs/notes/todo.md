# To-do

Small items that don't fit elsewhere. Delete each when done. See also the remaining layering
edges in [architecture.md](../architecture.md), the [bug notes](bug-reports/README.md) and the
[modules roadmap](modules-experiment.md).

## Build system and CI

- **ccache:** through `CMakeUserPresets.json` locally (`-Xclang -fno-pch-timestamp` and ccache
  sloppiness for the PCH), and as CI caching.
- **Presets:** `linux-{dev,asan,tsan}`, and workflow presets that CI calls directly. Windows ASan
  needs the MSVC runtime setup.
- **New CI jobs:**
  - Non-PCH build: the PCH hides missing includes (Windows clean Debug: 44 s with PCH, 50 s without).
  - ASan+UBSan.
  - TSan: it found three task service races (fixed). Needs suppressions for fmt's direct stdio
    buffer access (`race:fmt::v12::detail::glibc_file`, `race:fmt::v12::detail::file_print_buffer`);
    TSan symbolization of suppressed reports makes each run 1–2 minutes.
  - Optional newest-clang job.
- **Runner images:** `ubuntu-26.04` is still a public preview; move off it if it gets dropped.
- **Per-module CMake fragments** instead of the flat source lists in `include/CMakeLists.txt` and
  `src/CMakeLists.txt`.

## Third-party

- **SDL3:**
  - A consumer for text/IME events (preedit and committed text arrive, the UI ignores them),
    candidate events and `SDL_SetTextInputArea`.
  - Gamepad/joystick/sensor subsystems are compiled in but not initialized.
  - `no-dynapi.patch` is a choice: drop it to let users swap SDL.
- **License audit:** SDL compiles in code whose license is only in file headers, so it's missing from
  the generated `THIRD_PARTY_NOTICES.md`: the Wayland protocol XMLs, `stb_image.h`/`miniz.h`
  (`SDL_stb.c`), `xsettings-client.c`, `edid-parse.c`, `imKStoUCS.c`, `SDL_rotate.c`,
  `controller_type.c`, `libm/s_modf.c` (Windows). Needs an audit pass and a way to pull notices out
  of headers in `vendor.py notices` (or extra `license_files` cut from them).

## Code

- **`debug/thread_name.cpp`:** the Windows path looks `SetThreadDescription` up dynamically, then
  calls it directly instead of through the looked-up pointer. Call through the pointer, or drop the
  lookup if Windows 10 1607+ is a hard requirement anyway.
- **`extras`:** switch `function_ref` to `std::function_ref` once libstdc++ (16) and MS STL both ship
  it. `fixed_pool`, `refcnt_ptr`, `bitset` and `linear_allocator` are unused, kept for possible
  future use.

## Repo

- **README:** the project description (placeholder in place).
