# Design drafts

Directions for the engine's architecture that aren't implemented yet. They are intentions, not
decisions: rework them freely. What exists is described in [architecture.md](../architecture.md).

## Engine and plugins

The engine provides infrastructure, optimized primitives and frameworks, geared towards the
expected game specifics. All game logic lives in plugins, and the base game is a plugin itself:
**mods have exactly the same rights as the base game.**

- Parts: engine core (C++, infrastructure, common services), scripting API (language not chosen),
  base game (plugin), mods (plugins).
- Engine domains: rendering, physics, audio, network, world/scene hierarchy, input, scripting,
  messaging; supporting systems: file I/O, timing, jobs, logging, memory allocation, debugging.
- A plugin is a shared library plus an asset pack, linked against libvoxen (a shared library
  exporting its API with `VOXEN_API`) like the game executable and tests.
- Two ways to build plugins, the first is much easier to maintain:
  - drop the plugin source into the source tree and build with voxen's build system;
  - build against standalone engine headers and a prebuilt libvoxen with any build system.
- Split libvoxen for a headless server: core (extras, m, tk, root, os, debug, util, svc), sim
  (assets, land, world) and gfx (gfx, client). A game-on-top design comes after that.

## Rendering layers

`gfx/vk` keeps only the Vulkan wrappers, `gfx/graph` the render graph framework, and `gfx/passes`
the concrete render graphs and passes, which sit next to the wrappers today.

## Messaging

Design targets, still valid but not measured (there's no messaging benchmark yet):

- 100K messages per second without becoming a bottleneck;
- scaling to 32+ core CPUs, no critical sections in hot paths;
- small payloads (almost always below 128 bytes);
- ~1000 message types (an extremely heavily modded case);
- sending never blocks waiting for the receiver to process messages (that would deadlock), so
  bounded ring buffers are out.

`svc::MessagingService` superseded the original sketch:

| Original sketch | Implementation |
|---|---|
| Lock-free >99% of the time | Fine-grained locking: futex lock per inbound queue, 512 shards of routing tables behind futex RW locks |
| `u32` message IDs from a type registry, assigned at compile time for engine messages and at run time for plugins | Random 128-bit `UID` per payload type (`MESSAGE_UID`), no registry, plugins need no lookup |
| Fixed 32-byte trivially copyable payloads | Any payload type up to pointer alignment, allocated from pipe memory, with an optional deleter; empty types (signals) allocate no payload |
| ID-based dispatch to handlers is the user's job | Typed handlers registered per message type on `MessageQueue` |
| Many-to-one queues with a designated handler | `MessageQueue` registered per agent `UID`, `MessageSender` for send-only agents |
| Dispatcher keeps queues alive via `shared_ptr` and collects dead ones | Explicit RAII registration, queue storage reused |
| — | Request messages: shared payload, completion status, waiting and completion messages |

Still open from the sketch: **many-to-many routing.** Broadcast message types and handlers are
declared, but sending a broadcast (routing to subscribed agents) isn't implemented. Messaging
belongs to the trusted ring of engine subsystems, so type safety against deliberate misuse is
not a goal.

## Data and storage

- Assets: immutable, generally large (textures, shaders, meshes), stored as raw files or packs.
- Non-assets: mutable, generally small (binaries, configs, player profiles with settings and
  savegames), raw files only.
- Split config formats from data formats.

## Shipping builds

- Strip shader debug info: SPIR-V can't move it to a separate file like C++ debug info, so shipping
  needs a stripping step or a dedicated build option.
- A build option separating development features from optimization level, since development
  builds are profiled and debugged in Release too.

## Third-party dependencies

Keep a dependency only if it is fundamental, or highly modular and encapsulated, with no external
code on the hot path.

- **HarfBuzz**: when text layout comes, cache shaping per text run, never shape per frame.
- **zlib** (used by `tk::png`, tools and startup only): consider libdeflate (whole-buffer) or
  zstd/lz4 for chunk saves.
