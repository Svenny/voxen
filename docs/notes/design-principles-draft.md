# Design principles (draft)

A structured reading of the stream-of-consciousness notes from 2026-10-28, reshaped into
principles and decisions. Each item is marked **P** (principle: how we decide), **D** (decision
proposed, open to challenge), **S** (settled in discussion on 2026-10-28) or **?** (open question
that needs an answer before the ECS is built). "Today" refers to the state of the repository at
commit `93967d4`.

## 0. Identity and scope

- **P0.1 Target.** A C++ engine for compute-heavy, large-scale simulation: open worlds, sandboxes,
  many interacting entities. Infrastructure and primitives, not a content ecosystem.
- **P0.2 Expert tool.** Performance and flexibility over simplicity. No safety nets that cost
  performance; misuse is allowed to be slow. Required expertise is a feature, not a bug.
- **P0.3 Import, don't replace.** Where an external tool does a job well (DCC tools, image and
  audio encoders, compilers, version control), the engine imports its output.
- **P0.4 Everything is reworkable.** No compatibility concerns until a first release; design for
  what the architecture must eventually carry, implement what the next milestone needs.
- **P0.5 The scale test.** The reference workload is a 32-core machine with 100+ GiB of world state,
  tens of thousands of active machines at a stable fixed tick rate. Every design below is judged by
  whether it still works there, assuming equally careful gameplay code.

## 1. Simulation model

- **P1.1 Ticks are pure functions.** `state[i+1] = tick(state[i], inputs[i])`. Entity code reads
  only the immutable snapshot `i` and its own mutable copy; it never observes another entity's
  tick-`i+1` state.
- **S1.2 Reads are free, writes are messages.** Any entity may read any part of snapshot `i`
  directly (that is what immutability buys). Messages exist only to request changes to someone
  else's state or to signal events, and are delivered no earlier than tick `i+1`.
- **S1.3 In-flight messages are simulation state.** Outboxes produced in tick `i` are part of
  snapshot `i+1`'s input, so they are snapshotted, saved, replicated and rolled back together with
  components. This answers "what about the message queue when saving". It also rules out reusing
  `svc::MessagingService` as the entity bus: that bus is a process-level service, not state.
- **S1.4 Design-level determinism.** No design choice may intentionally make the simulation
  path non-reproducible: given the same snapshot and inputs, a tick produces the same result on
  any thread count, and thread assignment affects performance only. Message delivery order is
  defined by data (sender id, sequence), never by arrival time; scheduling skew (P1.7) is derived
  from state, never from measured load. Numerical sources (floating point, fast-math, SIMD
  paths, neural-network inference) are not all eliminable at acceptable cost, so systems are
  classed in two tiers: *strict* (bit-exact across runs on the same platform: integer and
  fixed-point logic such as machines, circuits, inventories, scheduling) and *tolerant*
  (physics, AI), and a dedicated test suite measures the magnitude of divergence of the tolerant
  tier over long runs. Consequences: replays and bug reproduction are exact for the strict tier
  and bounded for the tolerant one. The networking target is server authority with state
  replication and client-side prediction (S12.1); lockstep falls out of a fully strict sim but is
  a niche case, not a focus: it is less cheat-resistant and makes every client bear the server's
  full simulation load, which is at odds with large-scale sims.
- **S1.5 Ordering is a system property, not an entity property.** There is no guaranteed
  update order between entities, parents and children included. Intra-tick dataflow that needs
  an order (input → controller → animation → transforms → physics) is expressed as ordered
  *system phases* with an explicit dependency graph. Hierarchy is a component (`Parent`) plus a
  transform-propagation system that runs in its own phase; it is not an execution-order rule.
- **P1.6 Two compute models, one state model.** Bulk work (transforms, physics, visibility, crowds)
  is written as *systems* iterating component tables; sparse and event-driven work (machines,
  circuits, AI decisions, scripted behaviours) is written as *reactive entities* woken by messages
  or timers. Both operate on the same component storage. The design must not force the factory
  example into systems-over-arrays, nor the crowd example into per-entity actors.
- **P1.7 Scheduling policies.** Per entity-system pair: always, periodic (`T` ticks, with a
  phase offset derived from the entity id so periodic load spreads evenly), reactive (on message
  or timer). Strict vs relaxed timing is allowed only if the relaxation is deterministic (S1.4).
- **S1.8 Behaviour is state machines, not stacks.** A "wait for reply" inside entity logic spans
  ticks, so it is serializable state in a component, not a suspended native coroutine. C++
  coroutines remain the tool for service-level I/O and asset streaming (they are not simulation
  state). If a scripting layer offers `await`-style syntax for entities, it must lower to
  serializable state machines (source generation), never to runtime stacks.
- **P1.9 Global solvers are systems.** Physics, pathfinding, fluid or power-network solving are
  whole-world algorithms with their own internal parallelism. They consume snapshot `i` and
  produce components for `i+1` like any system; entities interact with them through components
  (a `RigidBody` is data, not a message exchange).
- **S1.10 One base tick rate.** All systems share one fixed tick rate (the lowest common
  multiple of the rates anyone needs); slower systems use periodic policies (P1.7). There are no
  secondary simulation clocks.
- **S1.11 The simulation never sees wall time.** The tick is fixed; overload is handled by the
  scheduler (time stretch: fewer ticks per wall second) without the simulation noticing. The
  render thread interpolates between snapshots using wall time. System calendar time may enter
  the simulation only as an explicit input (calendar-based events), never as a measure of tick
  duration. The simulated time per tick may still vary when a project needs it (slow motion,
  rewind): that is a controlled scaling multiplier passed in as tick input, never a measured
  duration.
- **D1.12 Timelines above the simulation.** Pause means there is at least one more timeline:
  the *session* (process lifetime, event-driven: menus, pause, time scale, which world is
  loaded, settings, profile and meta-state). The *presentation* timeline (frames: render,
  audio, UI) is another and need not coincide with the session one. Session values that affect
  gameplay (difficulty, rule toggles) enter the simulation only as explicit tick inputs, recorded
  in the snapshot like any input (S1.11), so a save or replay carries the difficulty in force at
  each tick; the sim never reads settings directly. The editor timeline (S9.4) is a fourth,
  stepped by `edit()`.

## 2. State storage (V8G and beyond)

V8G ("VersioninG") is the working name for the component storage backbone: copy-on-write values,
snapshots and diffs, whatever the implementation. `V8gHashTrie` and `V8gFlatMap` are two
implementations behind common concepts, helpers and storage policies; the name stays through the
redesign, whether or not any of today's code survives it.

- **P2.1 Snapshots are the integration contract.** Render, replication, saving, undo, tools and
  tests all consume immutable snapshots; nothing but the tick function ever sees mutable state.
- **S2.2 Copy-on-write granularity is pages, not objects.** Versioning of components works on
  pages (fixed-size blocks of N components of one type), with a page copied on first write per
  tick. The hash trie stays as backing storage for coarse objects (land chunks, regions, large
  assets) and is not the per-component store: the benchmark (P2.6 results below) measured
  ~105 ms per tick for 100k random updates against 7-12 ms for pages. The open cost of pages is
  retention: random writes touch every page, so a snapshot retains the whole table; page size
  and write locality (cells, P3.2) are the levers.
- **S2.3 One dense key per entity.** Entities (and components) are addressed by a dense key with
  an index:generation split (48:24 in 64 bits, or 64:64), used for tables, iteration, references,
  files and network alike. There is no per-entity random `UID`; `UID` remains the identity of
  things authored independently and merged without a central allocator: assets, prefabs,
  component and message types, services. Consequences: the key allocator's state (free list,
  generations) is part of the saved world, indices are never compacted, prefab-internal references
  are remapped at instantiation, and in multiplayer the server allocates keys while clients use a
  reserved range for predicted entities until confirmed.
- **P2.4 Diffs are first-class.** Every versioned structure can enumerate changes between two
  snapshots in time proportional to the change (`visitDiff` today). Replication, render upload,
  save deltas and editor change tracking are all diff consumers.
- **P2.5 Retention is explicit.** Snapshots stay alive while referenced; the cost is proportional
  to retained changes. Rollback netcode and editor undo are "keep N snapshots and replay".
- **P2.6 Measure before building on it.** A benchmark that updates 100k components per tick
  through the storage layer, with snapshot retention of 1, 4 and 64 ticks, is the gate for the ECS
  design. The storage half exists as `src/sandbox/bench_v8g.cpp` (target `bench-v8g`); its results
  are in "P2.6 results" at the end of this section. No messaging benchmark exists yet.
- **P2.7 Immutability is enforced, not promised.** The one real V8G consumer broke the contract
  until 2026-10-28: `editBlock` in `src/voxen/land/land_service.cpp` mutated a published chunk in
  place and then inserted a null value for its key. Published values must be `const` all the way
  down (snapshot access returns `const` only; mutation goes through a copy-on-write handle that the
  storage hands out for the current tick). Debug builds can additionally verify that no snapshot
  older than the current tick is written.

### P2.6 results: V8G hash trie vs paged copy-on-write (2026-10-28)

`bench-v8g` (`src/sandbox/bench_v8g.cpp`; `cmake --build --preset linux-release --target bench-v8g`,
then `build/linux-release/Release/bin/bench-v8g`) holds 1M components of 64 bytes keyed by a dense
64-bit index hashed with `xxh64Fixed`. Each tick overwrites N random keys, publishes a snapshot
(a copy of the container), diffs it against the previous one and drops the oldest so that R
snapshots stay alive. Two stores: `V8gHashTrie<DenseKey, Component>` with one `shared_ptr`
value per component (`insert(tick, key, makeValuePtr(...))`), and a baseline of pages of 1024
components plus 1024 per-item versions (72 KiB) held by `shared_ptr`, a page copied on its first
write in a tick, a snapshot being the vector of page pointers. Single thread, Ryzen 9 7845HX,
clang 21, `-O3` ThinLTO `-march=x86-64-v3`, mean of 24 ticks after a warm-up of max(4, R) ticks.
"Tick" is update + publish + retire; the diff is timed apart. Allocations count every global
`operator new`; "retained" is the heap a snapshot held alone, freed when it was dropped.

| updates/tick | retained | store | tick ms (mean / worst) | allocs/tick | retained MiB/snapshot | diff ms | peak RSS MiB |
|---:|---:|---|---:|---:|---:|---:|---:|
| 10k | 1 | hash trie | 13.7 / 15.1 | 34,824 | 8.3 | 3.2 | 161 |
| 10k | 1 | paged COW | 6.5 / 7.1 | 978 | 68.7 | 0.7 | 160 |
| 10k | 4 | hash trie | 15.1 / 16.2 | 34,824 | 8.3 | 3.2 | 190 |
| 10k | 4 | paged COW | 9.0 / 16.7 | 978 | 68.7 | 0.9 | 348 |
| 10k | 64 | hash trie | 15.2 / 15.9 | 34,805 | 8.3 | 3.3 | 698 |
| 10k | 64 | paged COW | 7.1 / 7.4 | 978 | 68.7 | 0.9 | 4,473 |
| 100k | 1 | hash trie | 105.3 / 116.1 | 241,236 | 41.0 | 19.0 | 467 |
| 100k | 1 | paged COW | 7.5 / 8.2 | 978 | 68.7 | 0.7 | 206 |
| 100k | 4 | hash trie | 104.9 / 111.9 | 241,236 | 41.0 | 19.5 | 329 |
| 100k | 4 | paged COW | 8.6 / 8.9 | 978 | 68.7 | 0.9 | 350 |
| 100k | 64 | hash trie | 103.1 / 108.0 | 241,366 | 40.9 | 19.8 | 2,891 |
| 100k | 64 | paged COW | 8.3 / 8.8 | 978 | 68.7 | 0.9 | 4,474 |
| 500k | 1 | hash trie | 273.6 / 288.4 | 766,090 | 80.1 | 30.8 | 1,766 |
| 500k | 1 | paged COW | 12.1 / 13.0 | 978 | 68.7 | 0.8 | 249 |
| 500k | 4 | hash trie | 272.7 / 286.4 | 766,090 | 79.9 | 30.7 | 512 |
| 500k | 4 | paged COW | 12.5 / 13.5 | 978 | 68.7 | 0.9 | 511 |
| 500k | 64 | hash trie | 255.1 / 269.5 | 766,169 | 79.8 | 30.5 | 5,593 |
| 500k | 64 | paged COW | 12.6 / 14.2 | 978 | 68.7 | 0.9 | 4,477 |

Of the trie's tick at 100k updates, 59 ms is the updates and 46 ms is publishing and retiring
(mostly freeing the ~241k allocations of the dropped snapshot); the split is similar at the other
sizes. Fill: 1M trie inserts take 270 ms and 2.03M allocations, the paged fill 15 ms and 979.

**Verdict on the budget.** The hash trie as a per-component store is out by an order of
magnitude: 100k random updates cost ~105 ms per tick, 6.5x a 16 ms tick, before any game logic
runs, and the diff of that tick is another 19 ms. Only the 10k case fits, and it fills the whole
budget (14-15 ms). Retention count barely changes the tick time, so the cost is the per-update
work itself: 2.4 allocations per update (one value, plus the copy-on-write of nearly every one of
the ~65k leaf-level nodes, each of which is hit by random keys every tick) and the matching frees
one snapshot later. Peak RSS is also inflated by fragmentation from this churn (1.7 GiB at
500k updates with 140 MiB live). The scaling is linear in the update count, so no tuning of
node size or root fan-out closes a 6.5x gap; S2.2 stands.

**Paged copy-on-write, by what factor.** Pages are 2x faster at 10k updates, 12x at 100k and
22x at 500k, with 978 allocations per tick at every size, and their diff is 20-35x faster. The
reason is that random keys are the worst case for pages: even 10k updates touch all 977 pages,
so every tick copies the whole 69 MiB table and the tick time is a memory-bandwidth bound copy
(7-12 ms) rather than per-update work. The same fact is the baseline's weak point: it retains a
constant 68.7 MiB per snapshot, 8x the trie at 10k updates and 1.7x at 100k; only at 500k does
the trie retain more. With 64 snapshots that is 4.4 GiB against 0.65 / 2.7 / 5.2 GiB. The paged
design is therefore within budget for the tick at every update count but needs its retention
cost addressed before it is adopted as is: smaller pages, pages sized per component type, or
cell-local update patterns (P3.2) that touch a fraction of the pages, all of which cut the copy
and the retention together. The benchmark should be re-run with a locality-aware key pattern
once cells exist, and this table should be read as the random-access worst case for both.

## 3. Parallelism

- **P3.1 Threads are invisible to simulation code.** Entities and systems never see thread ids,
  affinities or queues. The scheduler owns topology awareness (NUMA, cache sharing, core parking).
- **P3.2 Locality comes from data layout, not thread pinning.** The unit of scheduling is a
  *cell*: a spatial or logical group (chunk, region, explicitly declared group) whose entities are
  updated by one task. Messages between entities of one cell stay local; cells are the natural
  unit for work stealing, for save/stream granularity and for replication interest. Sticky
  thread assignment and dynamic communication-graph clustering are deferred until a profile of
  real workloads shows cells are not enough.
- **P3.3 Task system stays generic.** `TaskService` (counters, dynamic graphs, continuations,
  coroutines) is the only thread pool. Sim scheduling, asset streaming, render preparation and
  tools all launch into it; no subsystem owns threads except the ones with external constraints
  (OS main thread, GPU submission, audio callback).
- **P3.4 Tick barrier is the only global sync.** Within a tick, systems in independent phases
  overlap; the end-of-tick publish is the single point where the snapshot becomes visible.

## 4. Entities, components, scenes

- **S4.1 Entity = id; component = plain data; system = function.** Components are trivially
  serializable records (no pointers, no owning handles, references by entity key or asset id).
  Layout is defined in the schema (section 7), generated for C++ and for scripting.
- **D4.1a Project component layouts need an add-on rebuild, never an engine rebuild.** The
  engine stores project components opaquely (size, alignment, type UID, a generated function
  table for copy, serialize, diff and inspect), so a project component never requires an engine
  rebuild; P5.1 and P5.5 demand that. A layout change is a schema change: it regenerates code,
  rebuilds the project add-on (seconds, with hot-reloadable plugins per S5.2) and migrates live
  state through the document versioning path (P7.6): serialize out, reload, deserialize in.
  Dynamic-layout components (script- or DSL-defined, no rebuild) are not a second component kind:
  the schema's reflection tables, needed anyway by the editor, serialization and replication,
  give interpreted field access to any component, which covers tools and prototyping and costs
  only where used. Hot paths are typed code over generated layouts.
- **S4.2 Scene is a snapshot, prefab is a document.** A scene file is a serialized snapshot (set
  of entities with components). A prefab is a document whose instantiation is a DOM merge
  (base document + instance overrides), so prefab overrides, asset-meta inheritance and settings
  layering are one mechanism. Nested scenes are prefabs that happen to be large.
- **S4.3 Hierarchy is data.** `Parent` is a component (`parent key, order`) and the single
  source of truth. `Children` is not stored: it is a reverse index of `Parent` maintained from
  diffs (S4.5), which removes both the variable-length component and the two-sources consistency
  problem. `Transform` stays purely spatial and does not absorb the parent link: non-spatial
  entities (a machine's logic holder, groups, UI nodes) take part in the hierarchy for ownership
  and active/disabled propagation without carrying a transform, and a spatial entity resolves
  its world transform against its nearest spatial ancestor. Propagation (transforms, active
  state) is a system phase (S1.5). Streaming, LOD and replication ownership attach to cells
  (P3.2), not to the hierarchy.
- **D4.3a Variable-length data in components.** Not every component is a fixed-layout POD
  (inventories, names, scripted tables). The schema distinguishes *inline* fields (fixed layout,
  in the page) from *blob* fields: a handle to an immutable, reference-counted buffer copied on
  write, which the Immutable/Shared storage policies of V8G already model. Blobs keep components
  trivially copyable and snapshot-safe (a copy is a reference bump) at the price of an
  indirection; the rule that a component never points into another entity or component still
  holds.
- **D4.4 One paged table per component type, indexed by entity index.** With dense keys (S2.3)
  the archetype-vs-sparse-set choice mostly dissolves: every component type is a paged array
  indexed directly by entity index (a radix tree of pages over the 48-bit index space, presence
  bitmap per page), so a lookup is two loads, iteration walks the pages of one type and joins
  other types by index, and pages are also the copy-on-write unit (S2.2). Locality, which is
  what archetypes buy, comes from the key allocator instead: entities of one cell or one prefab
  instance get contiguous indices, so the pages of the types they share are dense and iterate
  like an archetype table. A schema-declared archetype is then an allocation hint plus a
  guarantee that its types are always attached together; a rarely attached component is a type
  whose pages are mostly absent. The explicit entity-to-components mapping is the entity header
  table (one more paged table, always present): generation, alive flag and the set of attached
  types as a bitset or, past a type count worth measuring, a small sorted list or archetype id.
  Adding or removing a component is a header diff plus a page write; nothing moves. To validate
  by extending the P2.6 benchmark with an entity-index radix tree next to the hash trie and the
  flat paged table it already measures.
- **S4.5 Indices are views of a snapshot.** Primary storage (V8G) is the only authoritative
  state; anything that answers a query faster is an index in the database sense: a spatial hash
  or BVH for "what is near me", the `Children` reverse index, name and tag lookups, "machines
  waiting for power" lists. An index is maintained by a system from diffs (P2.4) in a phase after
  its sources' writers, is published with the snapshot it describes (immutable and versioned
  like a component page) and is never saved or replicated: losing it is a rebuild, never data
  loss. An index is copy-on-write when it is large and rebuilt per tick when it is small; which
  one is a per-index decision measured against the P0.5 workload.
- **D4.6 Large-world coordinates from day one.** Retrofitting is the lesson of Unreal's LWC, so
  the sim transform is split from the start: an integer frame (cell coordinate, as
  `land::ChunkKey` already is) plus a local offset, and only the combined position crosses
  cells, saves or the network. Recommendation: `int32` frame on the chunk grid plus `float`
  local offset, rebased on cell crossing, which keeps a position 16 bytes and SIMD-friendly;
  `double` only where a solver integrates over many ticks (physics), and strict-tier logic on
  integer block coordinates anyway (S1.4). Render has no choice: camera-relative translation
  computed once per frame on the CPU (frame difference in integers, remainder in `float`), and
  `float` in everything that reaches a shader.

## 5. Scripting and plugins

- **P5.1 The plugin ABI comes before the language.** Component layouts, system registration,
  message types, scheduling policies and the snapshot access API are defined as a C-level data
  contract generated from the schema. C++ plugins are the first client; a managed language binds
  to the same contract. Nothing in the simulation model may depend on a language feature.
- **S5.2 C++ plugins first, C# second.** Hot-reloadable C++ plugins (shared libraries with
  plain-data components) carry the engine until the ECS has stabilized; C# arrives as the second
  binding, not as a prerequisite.
- **D5.3 C# shape, if adopted.** Components as blittable structs over engine-owned memory; systems
  and behaviours as methods invoked per cell, not per entity, to amortize interop; CoreCLR hosting
  in the editor for iteration, NativeAOT for shipping. GC pauses are a tick-budget risk: entity
  code should allocate nothing per tick in steady state. A custom runtime for C# is out of scope
  (the test surface is the BCL); a custom *compiler back end* for a subset is a later option.
- **P5.4 Script failures are contained.** A throwing or over-budget entity update is logged,
  quarantined (entity skipped, flagged) and does not kill the tick.
- **P5.5 Mods and the base game are peers.** The base game is a plugin; whatever the base game
  can do, a mod can do.

## 6. Services and messaging

- **S6.1 Two buses, two contracts.** `svc::MessagingService` is the control plane: low-frequency,
  cross-thread, process-local, request/reply, never part of simulation state. The entity bus is
  the data plane: batched per cell per tick, deterministic delivery order, snapshotted. Separate
  front ends and separate back ends; a unified back end would force the control plane's locking
  and allocation into the sim path or the data plane's batching onto control traffic.
- **P6.2 The control plane is also the tool protocol.** Service messages serialized through the
  DOM over a socket are the editor command interface and the agent interface (section 9). One
  message vocabulary, three transports: in-process, local socket, test harness.
- **P6.3 Services own lifecycle, not logic.** Services initialize, expose state and launch tasks;
  simulation logic does not live in services. `land::LandService` doing ticks is an interim
  state to dissolve into the ECS.

## 7. Documents, schemas and formats

- **P7.1 One DOM.** A single in-memory document model (replacing the separate value types in
  `svc::Config`, `tk::toml` and `tk::json`) with readers and writers per format, and a schema
  layer that validates, applies defaults, merges layers and drives codegen (`DOM ↔ struct`).
- **P7.2 Layered merge is the universal override mechanism.** Settings (defaults → user → saved),
  asset meta (template → directory → asset), prefabs (base → instance), platform variants: one
  merge algorithm with per-field policy (replace, concatenate, delete).
- **P7.3 Format by shape.** TOML for configuration, JSON5 for authored data definitions, JSON for
  interchange, custom text for high-volume machine-owned files (scenes). KDL only if JSON5 proves
  insufficient for DSL-like definitions.
- **P7.4 Ownership split is strict.** Human-owned files are never machine-written; machine-owned
  files are never hand-edited. Any file that needs both is two files in two layers (P7.2). This
  deletes the "preserve human formatting" problem.
- **P7.5 Merge-friendly by design.** Machine-owned text files are line-oriented with stable element
  order and one UID-keyed entity per record. A git merge driver that understands the format is
  the real solution; the layout only makes the fallback tolerable.
- **P7.6 Versioned from day one.** Every document carries a schema version; upgrades are DOM → DOM
  converters with a regression corpus of old files.
- **P7.7 Codegen is the plumbing.** New fields are declared once in the schema and reach C++,
  scripting, serialization, replication and the editor through generated code.
- **D7.8 Settings are one layered document.** Console variables, options and engine
  configuration are one schema-backed DOM document (today split across `svc::Config` and
  `RuntimeConfig`) with layers per P7.2: defaults → platform → user → save or session → command
  line. Per-field schema flags pick the consumer: restart-required, read by a service on the
  session timeline (render, audio), or *sim input* (D1.12), which turns a change into a tick input
  instead of a direct read; cheat and replicated flags cover multiplayer. There is no separate
  cvar system: a cvar is a settings field with a console path.

## 8. Assets

- **P8.1 Dual index.** Every asset has a UID and a mount-point path; references in data use UIDs,
  humans and tools use paths.
- **P8.2 Mount stack.** Mounts are ordered layers (archives below, loose files above); later mounts
  override earlier ones. Registration, hot reload and the editor's live view use the same
  resolution; there is no editor-only path.
- **P8.3 The asset build is a build system.** Source → cooked → packaged is a DAG with
  content-hashed inputs and declared dependencies (a model pulls textures, a scene pulls
  prefabs). Dependency tracking is therefore not optional: it is what makes rebuilds incremental
  and the nightly archive build possible.
- **P8.4 Loading is streaming.** Every load is an asynchronous task (coroutine) with partial
  results (LOD levels, texture tiles, chunk ranges) as first-class outcomes; synchronous load is a
  test-only convenience.
- **P8.5 Immutable assets, mutable documents.** Assets never change at runtime; documents (scenes,
  saves, profiles) do. The storage layer treats them differently (P7.4, design-drafts "Data and
  storage").

## 9. Tools, editor, agents

- **P9.1 Editor = player + plugins.** One binary; editor features are plugins using public APIs.
- **P9.2 Headless first.** The engine runs without a window or GPU (the core/sim/gfx library split
  from design-drafts). Tests, servers, asset cooking and agents all use the headless mode.
- **P9.3 Everything callable is a command.** Editor operations are messages (P6.2) with text
  encoding; the GUI is one client of them, an LLM agent another, a test another. Visual feedback
  is a command too (render a view to an image).
- **S9.4 Undo is snapshots.** Editor undo/redo retains snapshots (P2.5) instead of command
  inversion. This puts the editor outside the simulation timeline: an edited world is not
  stepped by `tick()` but by a second snapshot-emitting function, `edit(state, operation) →
  state'`, producing one snapshot per editor operation ("edit ticks", unrelated to sim ticks).
  Both functions consume and produce the same snapshot type, so play-in-editor is "start ticking
  from this snapshot" and stop is "discard back to the edit snapshot". Undo depth is a retention
  count, redo is the retained forward chain until a new edit replaces it.

## 10. Rendering and UI

- **P10.1 Render reads snapshots.** The renderer consumes the latest published snapshot and
  diffs (P2.4) to update GPU-side state; it never touches mutable simulation data and runs on its
  own timeline.
- **S10.2 Immediate-mode UI with derived state.** UI is rebuilt every frame from snapshots and
  input; whatever state it keeps (focus, scroll, text edit, animation) is derived, discardable and
  keyed by stable ids. The claim is "no authoritative UI state", not "no state".
- **S10.3 Multi-backend render graph from day one.** The state of Windows Vulkan drivers
  (`docs/notes/bug-reports`: AMD crashes and silent zeros on pointer use, Intel crashes on 16-bit
  loads; all fine on Linux) makes a second backend (D3D12) a shipping requirement. The
  abstraction boundary is the render graph: resources, passes and dependencies are declared
  once, barriers and transient allocation are derived, and a backend only executes graphs. Of
  the two drafts (`include/voxen/gfx/graph/`, the Vulkan-specific
  `include/voxen/gfx/vk/render_graph_*.hpp`) the backend-neutral one is the direction. Shaders
  stay Slang, which targets SPIR-V and DXIL; the consequence for shader style is bindless
  descriptors and structured buffers as the portable path, with buffer-device-address pointers
  at most an optional Vulkan-only fast path, since DXIL has no equivalent and pointers are
  exactly what the drivers break on.
- **D10.4 One GPU layer, many consumers.** Rendering, ray-traced audio, neural inference and GPU
  physics all want the device, so one common layer serves all of them: device, queues, memory
  and a graph of work that is not render-specific (render graphs become work graphs), with
  shared structures (ray-tracing BVHs of the scene) falling out of it. A separate device (iGPU
  for compute) is a deployment option the layer allows, not an architecture: the common machine
  has one GPU, so contention is managed with queue priorities and per-consumer budgets rather
  than avoided. Sim-side consumers get a stricter rule: a tick never waits for the GPU. Results
  enter the simulation pipelined, N ticks later, as inputs (the pattern of messages and D1.12),
  their producers are tolerant-tier by definition (S1.4), and every such consumer has a CPU
  fallback or is optional, because headless servers and tests (P9.2) run without a GPU.
- **D10.5 Passes in C++, pipeline in data.** The render pipeline is neither fully baked nor
  fully scripted: passes are C++ (plugin code, hot-reloadable, S5.2); the graph that assembles
  them is a document (P7.1) that projects and mods compose, reorder and parameterize without an
  engine rebuild; shaders are hot-reloaded assets. A scripted pass language is a non-goal: C++
  plugins already give the iteration speed, and the render thread's budget has no room for an
  interpreter. The base renderer is itself such a plugin (P5.5): whatever it can do, a project
  renderer can do.

## 11. Build and distribution

- **P11.1 Three builds.** Engine + project add-ons → binaries and SDK (per commit); loose assets →
  archives (nightly, on a file share); binaries + archives → redistributable (strip and package).
- **P11.2 Source compatibility within a major version**, no ABI promises. Plugins build in-tree
  with the engine's build system by default.
- **P11.3 Development builds are profiled.** A development/shipping switch is independent of the
  optimization level (design-drafts "Shipping builds").

## 12. Networking

- **S12.1 Networking model.** Server-authoritative state replication with client-side prediction
  is the main target; snapshot diffs (P2.4) are the server-side delta source, retained snapshots
  (P2.5) the base for reconciliation. Lockstep is a niche fallout of a fully strict sim (S1.4).
- **D12.2 Interest is cells.** The unit of interest management is the cell (P3.2): a client
  subscribes to a set of cells (around its avatars, plus whatever the game pins: a remote camera,
  an owned base) with hysteresis on the boundary, and receives per-cell diffs. The server never
  evaluates per-entity relevance; a cell is in or out. Replication priority and bandwidth
  budgeting are per cell per client (nearest first, stale-longest first), so the P0.5 world
  with thousands of machines costs a client only the cells it looks at.
- **D12.3 Clients send inputs, never state.** The wire carries the client's input records
  (P15.1) up and cell diffs down. Prediction scope is the client's own avatar and what it
  directly manipulates in the same tick (strict-tier interactions it initiates: a door, a
  pickup); everything else is interpolated from server snapshots and never predicted.
  Reconciliation is "retain snapshots, re-tick from the last confirmed one with the pending
  inputs" (P2.5), which bounds the retention count by round-trip time in ticks.
- **D12.4 The protocol is generated.** Diff encoding, input records and the session handshake
  come from the schema (P7.7) with a protocol version derived from the schema hash; mismatched
  versions refuse to connect rather than negotiate. One unreliable-ordered channel with
  acknowledged baselines for cell diffs, one reliable channel for session control. Transport
  (UDP, a relay, in-process loopback for tests) sits below both.
- **D12.5 Mods on the server are the trust model.** Simulation mods run on the server; clients
  must have the same mod set (checked in the handshake) for prediction and presentation, and a
  mismatch is a refusal, not a download. Native plugins are not sandboxed (P5.5): installing a
  mod is trusting its author, as on every other PC platform.

## 13. Audio

- **P13.1 Audio is a presentation consumer.** Like render (P10.1), audio reads snapshots and tick
  events, interpolates with wall time and runs on its own timeline: a real-time callback thread
  fed lock-free from presentation-side tasks, which never waits on the sim or the task system.
- **P13.2 Import the mixing, own the propagation.** Mixing and mastering are solved outside the
  engine (DAW-authored content; an existing backend and a borrowed mixer design rather than our
  own). The engine's own ambition is physically based propagation modelled live from the world:
  line-of-sight occlusion, reverb, echo and delay, Doppler; "ray-traced audio". That is a GPU
  compute consumer (D10.4) and one more reason to share acceleration structures with rendering.
- **?13.3** Nobody on the project has mixing or mastering experience. The first audio milestone
  is a conventional mixer graph (buses, snapshots, ducking) with a design borrowed wholesale,
  before any propagation work starts.

## 14. Physics and collision

- **P14.1 Physics is a tolerant-tier system.** One solver step per tick on the fixed rate
  (S1.10), reading snapshot `i` and writing components for `i+1` (P1.9), with its internal
  parallelism on the task system (P3.3). The authoritative state is components (`RigidBody`,
  `Collider`, `Transform`); the solver's own world (broad phase, contact cache, islands) is an
  index (S4.5) kept in sync from diffs, not the truth.
- **D14.2 Import the solver.** A continuous rigid-body solver is not engine work; Jolt is the
  candidate (multithreaded, cross-platform deterministic per binary, optional double precision,
  save/restore of its internal state). The import boundary is the index framing above, and the
  open point is rollback (D12.3): a solver whose internal state cannot be restored to snapshot
  `i` makes predicted bodies diverge, so solver state save/restore is a selection criterion.
- **D14.3 Voxels are their own collision shape.** Terrain collision queries the chunk storage
  directly through a custom shape, never a cooked mesh; a chunk edit is collision-visible on
  the next tick with no bake step. Large-world positions (D4.6) mean the solver works in
  cell-local coordinates and bodies rebase on crossing, or the solver is instanced per region.
- **P14.4 Two physics.** Grid-aligned machine logic (belts, fluids in pipes, circuits) is
  integer, strict-tier logic in gameplay systems (S1.4), never the continuous solver. The
  solver is for bodies that move freely; the two meet only through components.

## 15. Input

- **P15.1 Devices never reach the simulation.** Raw device events (SDL today) are mapped on the
  presentation side to *actions*: project-declared in the schema, rebindable through the
  settings document (D7.8). Per tick the mapping produces an *input record* (action states and
  edges, tick-stamped, per player); that record is the `inputs[i]` of P1.1 and the only input
  the simulation ever sees. Immediate-mode UI (S10.2) consumes events before the mapping,
  so focus and capture are a presentation concern.
- **P15.2 Input records are the replay format.** Recording the records (plus the initial
  snapshot) is a replay; it is exact for the strict tier and the measuring stick for the
  tolerant tier (S1.4). The same records are the client's upstream traffic (D12.3) and the
  scripted driver for tests (section 19).

## 16. Persistence, streaming and replays

- **P16.1 A save is a snapshot plus its inputs.** Components, in-flight messages (S1.3), the
  key allocator (S2.3), session values in force (D1.12) and the tick id; never the indices
  (S4.5). Schema-generated binary with a version (P7.6) and a text export for diffing and
  inspection (P7.3). The tick function must be able to resume from a save bit-for-bit as it
  would have from the live snapshot.
- **D16.2 Saves are journals of cells.** The P0.5 world cannot be written whole, so the unit of
  persistence is the cell (P3.2): a save is a base plus an append-only journal of cell diffs
  (P2.4) with compaction in the background, and autosave is "flush the dirty cells". Land
  chunks are already shaped this way; entities follow.
- **D16.3 Streaming is the same mechanism.** A cell outside every interest set (players,
  pinned loaders) is written out and dropped; reloading reads the same format. Unloaded cells
  do not tick, and catch-up on reload is an explicit game rule (nothing, or an approximation),
  never implicit "simulate the elapsed time". The loaded set is itself derived from state, so
  it is reproducible (S1.4).
- **P16.4 Replays are first-class.** A replay (P15.2) is also a bug report (P18.3), a test
  (P19.1) and the base of a spectator or demo mode; the format is the save format plus input
  records, so there is one serializer.

## 17. Memory

- **P17.1 Budgets are explicit and per subsystem.** Snapshot retention (P2.5), component pages,
  loaded cells, GPU resources, asset cache and transient per-tick memory each have a budget,
  and the budget view is a tool, not a heuristic: every allocation is attributable to a
  subsystem tag. The sim's steady-state budget is dominated by retention count times the
  per-tick change, so retention is the first knob.
- **D17.2 Arenas, not a global allocator.** No engine-wide replacement of `operator new`
  (hooking it is a sandbox trick, P2.6). Subsystems allocate from their own arenas: pipe memory
  for messages and tasks, scratch memory for per-tick temporaries, pools for objects, and a
  page allocator for component pages (fixed size, later NUMA-aware, P3.1). Entity code
  allocates nothing per tick in steady state (D5.3 says the same for C#); a per-tick allocation
  in a system is a profiler finding.
- **P17.3 The working set is cells, not the world.** Memory scales with the loaded cells
  (D16.3), not with the world; the 100 GiB of P0.5 is on disk, and the budget decides how
  much of it is live.

## 18. Observability: logging, profiling, crashes

- **P18.1 Profiling is always on in development builds.** Instrumentation of ticks, phases,
  tasks and cells (P11.3) is built in, not a debug add-on, exported to an external viewer
  (Tracy-style) rather than an in-house one (P0.3). A tick over budget is an event with its
  phase breakdown attached. Timings feed tools only, never the sim (S1.4).
- **P18.2 Logging is structured and categorized.** Per-category levels, per-world and per-tick
  context on sim-side messages, a ring buffer in shipping builds that is dumped on a crash,
  and machine-readable output as an option for agents (P9.3).
- **P18.3 A crash report is a replay seed.** Because the sim is reproducible, the useful crash
  payload is the last retained snapshot plus the input records since, together with the stack
  and the log ring: a crash can be re-ticked under a debugger. Shipped builds send that only on
  opt-in, with no identifying data beyond a build id and the hardware class.
- **P18.4 Invariants are checked, not assumed.** Development builds verify the contracts that
  the design relies on: no write to a published snapshot (P2.7), deterministic message order,
  no per-tick allocation in systems, index consistency with its source (S4.5). Shipping builds
  compile them out.

## 19. Testing and benchmarks

- **P19.1 The sim is tested as a pure function.** A test is a scenario document (initial
  snapshot or prefab set), input records and an expectation: a strict-tier state hash, or a
  property checked on the resulting snapshot. Tests are data files run by one harness, so
  agents and tools (P9.3) can write them.
- **P19.2 The determinism suite is a gate.** The same scenarios run with 1, 4 and 32 threads
  and with shuffled cell-to-task assignment: strict-tier hashes must match, tolerant-tier
  divergence is measured against per-system thresholds over long runs (S1.4). It runs nightly
  with the benchmarks, not per commit.
- **P19.3 Benchmarks are tracked, not just run.** The P0.5 scale scenario (synthetic machines
  and crowds) and the storage benchmark (P2.6) report numbers that CI records over time, with
  thresholds that fail on regression; ad-hoc measurements live in `src/sandbox/`.
- **P19.4 Rendering is tested by image.** Headless rendering (P9.2) to an image with per-backend
  (S10.3) tolerances, since no two drivers agree bit-for-bit; a mismatch beyond tolerance is a
  failure, within it a warning with the diff image kept.

## 20. Localization

- **P20.1 Text is an asset, keyed, layered.** User-visible strings are never literals: they are
  keys into per-locale string tables that are assets (section 8) in a locale mount layer
  (P8.2), so a translation is an overlay and mods localize like the base game. Message
  formatting (plurals, gender, number and date formats) uses a standard format, not
  `fmt`-style positional substitution.
- **P20.2 Text rendering is not Latin-only from day one.** Shaping and bidirectional text are
  in the UI stack from the start (HarfBuzz and FreeType are already dependencies); fonts are
  assets with fallback chains per script.

## 21. Worlds and process lifecycle

- **D21.1 A world is an object, not a global.** Play-in-editor (S9.4), a server hosting several
  instances, and tests all put more than one simulation in one process. Services are
  process-level (section 6); a world owns its timeline, snapshots, cells and key allocator, and
  every snapshot and message carries its world. Nothing simulation-side is a singleton.
- **P21.2 Startup is services, then worlds.** The engine constructs services (P6.3) from the
  settings document (D7.8), then worlds from documents (S4.2); shutdown is the reverse, and a
  world can be destroyed and recreated without restarting services (editor reload, server map
  change).

## 22. Remaining domains, in one line each

- **D22.1 Procedural generation is strict-tier.** Generation is a pure function of seed and
  cell, bit-exact across platforms, because it is content: two players must see the same world.
  Floating point in generators is therefore fixed-point or integer, or tested as strict.
- **D22.2 Navigation is an index, pathfinding a solver.** The navmesh or voxel nav-grid is an
  index (S4.5) over the world, rebuilt incrementally from chunk diffs; pathfinding is a global
  solver (P1.9) answering requests as messages; neural inference for agents is a GPU consumer
  (D10.4) in the tolerant tier.
- **D22.3 Animation splits across timelines.** The sim owns animation *state* (clip, time,
  blend weights, as components, strict-tier where gameplay depends on it: hit frames); the
  presentation evaluates poses from that state per frame and never feeds back.
- **D22.4 Scripting debugging goes through the control plane.** Breakpoints, watches and
  stepping of entity logic are commands (P9.3) on a paused world (D1.12); native debuggers
  attach to C++ plugins as usual, and `edit()` (S9.4) is how a tool mutates a paused world.
