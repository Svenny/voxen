# Architecture

## Source layout

Every module has its headers in `include/<module>/` and its sources in `src/<module>/`.
Engine modules are subdirectories of `voxen/` with a namespace of the same name
(`voxen::os`, `voxen::svc`...), except `util/` and the root, which are plain `voxen::`.

| Directory | What belongs there |
|---|---|
| `extras/` | STL-shaped header-only extensions of `std::` (snake_case, `extras::`) that make sense in any C++ project. Depends only on the standard library. |
| `m/` | Header-only HLSL-style math: vectors, matrices, quaternions, transforms, GPU packing and color formats. Depends only on the standard library; fmt only through the opt-in `m/format.hpp`. |
| `tk/` | Toolkit static library linked into every binary: tools, engine, tests and plugins. Std-only public API with errors returned as values. May use `extras`, `m` and vendored libraries privately, never `voxen`. Only for code that tools need, or formats and contracts shared between the engine, tools and data (e.g. `tk::UID`). fmt only through the opt-in `tk/format.hpp`. |
| `voxen/` (root) | The engine prelude, included by nearly every module and plugin: export macros, version, logging, exceptions and error codes, `UID`. |
| `voxen/os/` | Thin platform layer: one interface over per-OS implementations (POSIX/Win32 syscalls, SDL windowing). Knows no engine concepts. |
| `voxen/debug/` | Debugging aids: crash reporting, thread names, UID registry. |
| `voxen/util/` | Domain-agnostic engine building blocks in plain `voxen::`: containers, allocators, pools, algorithms, timeline IDs. Never knows about services or any domain module. |
| `voxen/svc/` | The `Engine`, the service framework (locator, messaging, tasks) and engine-wide runtime state set up by `Engine` (config, file manager). |
| `voxen/assets/`, `land/`, `world/`, `gfx/`, `client/` | Domain modules: assets, voxel terrain, world simulation, rendering, the game client. |

Choosing between `util`, `tk` and `extras`: if a tool needs it or it's a contract shared with
tools and data, use `tk`. If it is std-shaped and engine-agnostic, use `extras`. If it is
engine-styled or needs logging, `VOXEN_API`, `os` or `debug`, use `util`. Being a pure function
is not a reason for `tk`.

`tk` is a public dependency of libvoxen, so being a static library it gets linked into
libvoxen and also into every binary linking it. That is safe only because **`tk` has no mutable
global state, no singletons and no initialization**.

## Dependency order

A module may include only modules to its left (and itself):

```
extras, m  <  tk  <  root ≈ os  <  debug  <  util  <  svc  <  assets  <  land  <  world  <  gfx  <  client  <  game
```

- `root ≈ os`: os headers may include root headers, never the reverse. Sources may cross between
  them (`log.cpp` needs the thread ID from `os/process`, os wrappers log their failures).
- Tools (`tools/`) link only `tk` and third-party libraries, never the engine.

Legacy edges still to be removed:

- `gfx -> client`: the legacy render graph draws from `client::GameView`.
- `svc -> gfx`: `RuntimeConfig` holds `gfx::GfxOptions`, filled by `Engine` from the command line.
