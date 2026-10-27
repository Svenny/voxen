# Voxen

<!-- TODO: project description -->

## Prerequisites

Curently, x86-64 Linux and Windows are targeted. Only the Clang compiler and Ninja build system are supported.

Common to both platforms:

- Clang 21 or newer
- CMake 4.2 or newer, Ninja
- Python 3.14 or newer
- Git LFS (see [below](#git-lfs))
- clang-format 21, optional: formatting is checked by CI

### Linux

Using Ubuntu 26.04 as the baseline. Debian has the same package names,
but check that `clang` and `cmake` are new enough:

```sh
sudo apt-get install clang cmake ninja-build mold pkgconf python3 git-lfs
sudo apt-get install libwayland-dev libxkbcommon-dev xorg-dev
sudo apt-get install libdecor-0-dev libdbus-1-dev libudev-dev
```

- `libwayland-dev` brings `wayland-scanner` (`libwayland-bin`), the build requires it
- mold is the default linker; without it the build falls back to LLD (`lld`)
- libstdc++ from GCC 14 or newer (for `std::stacktrace`), the default with `clang` on Ubuntu 26.04

### Windows

- Visual Studio 2026 or Build Tools for Visual Studio 2026 with the "Desktop development with C++"
  workload (MSVC headers and libraries, Windows SDK); the VS developer environment is not needed
- LLVM 21 installed in its default location `C:\Program Files\LLVM` (e.g. `choco install llvm --version=21.1.8`)
- `cmake`, `ninja`, `python` and `git` on `PATH`

## Building

Configure, build and test (`-debug` or `-release`):

```sh
# Linux
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug

# Windows (one multi-config build directory)
cmake --preset windows-ninja
cmake --build --preset windows-ninja-debug
ctest --preset windows-ninja-debug
```

The "game" target lands in `build/<configure preset>/<Debug|Release>/bin/`, e.g.
`build/linux-debug/Debug/bin/game` or `build\windows-ninja\Release\bin\game.exe`,
with the compiled assets next to it in `data/`.

## Source layout

Module rules and their dependency order are described in [docs/architecture.md](docs/architecture.md),
naming and coding conventions in [docs/conventions.md](docs/conventions.md). [docs/notes/](docs/notes/)
holds design drafts, known bugs, the C++ modules experiment and small to-do items.

## Git LFS

Built-in assets in `data/` and the prebuilt host tools in `toolchains/` are stored
in Git LFS, on the server set in `.lfsconfig`. Run `git lfs install` once before cloning,
or `git lfs pull` after cloning without it. To fetch only the host platform's tools:
`git lfs pull --exclude="toolchains/*/windows-*"` (or `linux-*` on Windows).

Without the LFS objects the files are small text pointers: configuring fails like
`slangc ... is a Git LFS pointer`; we try to diagnose these errors where possible.

## Third-party code

Dependencies are vendored, nothing is fetched at build time. `3rdparty/vendor.toml` lists the
sources built in-graph (`3rdparty/<name>`, with downstream patches in `3rdparty/patches/<name>/`),
`toolchains/vendor.toml` defines the prebuilt tools. Don't edit vendored files by hand:
change the manifest or a patch and run

```sh
python3 tools/scripts/vendor.py update <name>  # re-derive 3rdparty/<name> and the notices
python3 tools/scripts/vendor.py check          # what CI runs: the tree matches the manifests
```

## License

Voxen is licensed under the MIT license, see [LICENSE](LICENSE).
Third-party components and their licenses are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
