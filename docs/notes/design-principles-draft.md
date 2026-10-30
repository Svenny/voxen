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
- **P1.1a The readable window is one snapshot unless a project widens it.** A system may read the
  last `H` snapshots, `state[i-H+1..i]`, for a per-world constant `H` whose default is 1: then P1.1
  as written is the contract and nothing below applies. A project that needs the past (lag
  compensation resolving a shot against the snapshot the client was looking at, D12.3) opts in to a
  larger `H` sized from the longest compensated round trip in ticks, high tens to low hundreds at
  100+ ms and 128 Hz, and by opting in accepts what that costs: the window is retained by P2.5 as a
  hard minimum, not a policy; a save or crash seed (P16.1, P18.3) either carries it or declares it
  empty, in which case history readers see "no history" and degrade deterministically (the shot
  resolves against tick `i`); tests (P19.1) start with an empty window unless the scenario provides
  one. Nothing mutates history: the window is as immutable as any snapshot, which is why it costs
  nothing beyond retention (about 60 KiB per tick for a 64-player arena, under 10 MiB for 250 ms at
  128 Hz) where conventional engines build a position-history subsystem.
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
- **S1.4a Tier propagates through the data flow, and the tier mix is a project decision.** A
  strict-tier system that reads a tolerant-tier component is tolerant from that tick on: divergence
  enters through every threshold it evaluates and is amplified by the discrete decisions downstream,
  so the bounded divergence P19.2 measures holds only for data that never crosses back into a
  decision. Bounded does not mean a replay blows up in two ticks; it means two replays from one
  snapshot eventually diverge, and how long "eventually" must be is what a project decides and pays
  for. Design-level determinism (S1.4) is the engine not closing the door on its side; strict
  replays over long horizons are a real authoring and debugging cost that must pay off. A shooter
  whose replays are evidence (D12.5a) makes movement and hit registration strict (D14.5, D14.6) and
  leaves ragdolls tolerant. A city builder whose bug reports are the last minute before a crash
  (P18.3) may keep float traffic and accept that an hour-old save does not replay bit-for-bit, and
  chasing a fully strict city sim is not a goal of this engine. The engine's part is to make the
  propagation visible: the schema records each type's tier, the determinism suite reports where
  tolerant data enters strict decisions, and a project traces what reads a system's outputs before
  classing it. Corollary for D10.4: integer GPU compute entering at a fixed N-tick lag is
  strict-eligible; "tolerant by definition" is for floating-point GPU work only.
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
  These policies are also the engine's half of *simulation LOD*: an unwatched region can run its
  systems on longer periods with a larger step. The other half, behaviour-level LOD (an abstract
  "went to work, earned 200" instead of pathing there), is game logic, and the attention that
  drives both (which lots or regions are watched) is session state (S1.12) entering as a
  recorded tick input, so the outcome legitimately depends on what was watched and the replay
  still reproduces it.
- **P1.7a Any recorded input may drive LOD, including the time scale.** Attention (P1.7) is one
  driver of simulation LOD; the time-scale multiplier (S1.11) is another. A project declares per
  system the period and step for each speed setting (traffic at 4x: every second tick with a doubled
  fixed-point step, or the abstract form), so fast-forward is cheaper per simulated second rather
  than a stretched tick that silently runs at 2x, which is the "speed 3 is speed 2 on a big city"
  complaint reproduced by construction. The speed is in every tick's input record, so a replay
  reproduces the choice (S1.4); time stretch stays the backstop when even the reduced form does not
  fit.
- **S1.8 Behaviour is state machines, not stacks.** A "wait for reply" inside entity logic spans
  ticks, so it is serializable state in a component, not a suspended native coroutine. C++
  coroutines remain the tool for service-level I/O and asset streaming (they are not simulation
  state). Content-heavy games (a Sims-like with a thousand interactions, a narrative game with sixty
  thousand dialogue nodes) cannot author behaviours as hand-written state machines, so a
  serializable form of sequential code is a deliverable of the first content milestone, not an
  option. The two forms are not interchangeable. The *bytecode VM* is the form for content-authored
  behaviour (dialogue, interactions, quests): its source is a document (P7.1, a UID per node, so
  tooling can diff, merge, extract strings and analyse reachability), the asset build (P8.3)
  compiles it, its labels are the source UIDs so a running frame can be relocated after a reload
  (P15.2a), its frames and stacks live in blob components (D4.3a), and it is designed for content
  primitives (visit counts, hubs and returns, diverts, choice generation), not only for `await`. A
  *DSL* with `await`-style syntax lowered to state machines by source generation is the form for
  programmer-authored behaviour in C++ plugins, where a rebuild per edit is acceptable. Both forbid
  *native* stacks, not serializable ones. Cancellation is part of the same deliverable: an
  interrupted behaviour unwinds through declared cleanup steps, not by being dropped mid-state.
  Failure policy is per kind (P5.4): a content VM logs and continues (a failed condition is false, a
  failed effect is skipped) and only native entity code is quarantined, or a writer's typo stops the
  game.
- **P1.9 Global solvers are systems.** Physics, pathfinding, fluid or power-network solving are
  whole-world algorithms with their own internal parallelism. They consume snapshot `i` and
  produce components for `i+1` like any system; entities interact with them through components
  (a `RigidBody` is data, not a message exchange).
- **P1.9a Solvers declare their throughput.** A global solver answering requests (D22.2) serves at
  most a declared number of requests per tick, drained in data-defined order, with the backlog in
  its own component so it is state (S1.3). The cap is a schema constant tuned against the P0.5
  machine, never a measured budget (S1.4): a smaller machine stretches time (S1.11) rather than
  desynchronizes. A burst beyond the cap (five thousand repaths after a highway demolition) shows as
  queue length in the profiler (P18.1), never as a dropped request, and the game decides what a
  waiting agent does meanwhile.
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
- **D1.11a Ticks and frames are on demand when nothing moves.** The "when" of S1.15 is fixed-rate or
  demand-driven. In the latter the scheduler runs step `i+1` only when snapshot `i` has pending
  inputs, in-flight messages (S1.3), a due timer, or an entity under an always or periodic policy
  (P1.7); all of these are state, so the decision is deterministic (S1.4) and the tick id sequence
  has no gaps. A replay of such a world costs its non-empty ticks only (a twenty-hour narrative
  playthrough is 20k ticks, not 4.3M empty ones). The presentation timeline (S1.12) has the same
  switch: a frame is produced when a new snapshot was published, a UI-derived animation (S10.2) is
  running or input arrived, otherwise the process blocks on the event loop (a laptop GPU redrawing
  an unchanged dialogue screen at 144 Hz draws 5-15 W for nothing); audio keeps its callback
  (P13.1). An event-driven game selects both; a continuous sim selects neither.
- **S1.12 Timelines are the session loop plus configurations of one step function.** The *session*
  (process lifetime, event-driven: menus, pause, time scale, which world is loaded, settings,
  profile and meta-state) is the loop that drives everything else and has no snapshots of its own.
  The *presentation* timeline (frames: render, audio, UI) runs on its own clock and need not
  coincide with the session's. Every world timeline (play ticks, editor steps, a paused world that
  still accepts building) is the one step function of S1.15 under a configuration; there is no
  separate editor timeline, only the editor configuration. Session values that affect gameplay
  (difficulty, rule toggles, time scale) enter a world only as explicit tick inputs, recorded in the
  snapshot like any input (S1.11), so a save or replay carries the difficulty in force at each tick;
  the sim never reads settings directly.
- **P1.13 One writer per component.** In any tick a component instance is written by exactly one
  party: the system that owns the type in its phase, or the owning entity's own logic. Everything
  else reads it (S1.2) or sends a message (S1.3). Control therefore flows by the controlled reading
  its controller: an interaction entity that drives two interacting entities reads both freely and
  writes only itself, and each interacted one's movement and animation systems read "my interaction
  says go here, do this". This is what makes free reads safe without locks and keeps the writer of
  any value identifiable in a diff. A content runtime (S1.8) is the system-side case of this rule:
  it owns every component type it exposes to content as a variable, on whatever entity that
  component sits, so the effects of one behaviour step apply in program order and a condition that
  follows an effect reads it in the same tick (`kim.trust += 1; if kim.trust > 3` is one choice, not
  ten ticks of message hops). Other systems and entities change those variables only by message to
  the runtime (S1.2); the schema declares which types a runtime owns.
- **D1.14 Same-tick dataflow is a phase order or a solver, never a message chain.** Where players
  expect intra-tick evaluation order (an inserter sees the belt it feeds this tick, a signal network
  settles before the piston reads it), one message hop per link (S1.2: a tick each, 33 ms per
  hand-off, a second across a thirty-stage line, unplayable for a timing contraption) is the wrong
  model, and P1.6 promises not to force the factory into systems-over-arrays. The draft's own tools
  answer it. A system may read, in its phase, components written by an earlier phase of the same
  tick: that is what the phase graph (S1.5) exists for and the one place P1.1's "never observes
  `i+1` state" is relaxed, for systems in declared order, never for entities. A connected network
  (belt lanes, signal nets, power grids) is an index (S4.5) of connected components rebuilt from
  grid and component diffs, and one system (P1.9) owns the per-node component for the whole network,
  evaluating each component in a defined order inside its phase, one task per component batched by
  the cell of its lowest key (a 50k-node grid is about 1 ms serial). Entities at the edges see the
  result next tick as usual. Networks are therefore never cells: D3.2a's logical cells are for
  ownership (a vehicle interior), and a machine that sits in a power grid, a pipe network and a
  signal net at once belongs to one cell and three indices.
- **S1.15 One step function, five parameters.** `tick()` and `edit()` (S9.4) have the same shape,
  snapshot plus inputs to snapshot, and are one function configured by: *when* to step (fixed-rate,
  or demand-driven when nothing moves, D1.11a); *how much simulated time* advances (one base period
  times the scale multiplier of S1.11, or zero); *which phases* run, each system declaring the
  configurations it takes part in (transform propagation, index maintenance, cell assignment and
  invariant checks in all; message delivery, integrators and periodic clocks only when time
  advances; snap-to-ground and prefab instantiation only in editing); *which inputs*, always input
  records (P15.1), editor operations being commands in the same format (P15.1a); and *retention*
  (P2.5: every snapshot for undo, a handful for play). A *step counter* increases on every step and
  is the snapshot's version; the *tick counter* below it increases only on time-advancing steps and
  drives sim time, so a save or replay that spans pause, edit and resume records both kinds of input
  in step order. In-flight messages (S1.3) are not delivered by a zero-time step and survive it
  untouched. A project then chooses per action whether a change is a tick input (time advances while
  it happens) or an edit input (it does not): block placement in a sandbox is a tick input; a Sims
  build mode is edit inputs on a paused sim; a level editor is edit inputs on a sim that never
  ticked; a city builder that builds while paused steps at zero time with its input-applying phases
  on and its integrators off, so build commands stay tick-stamped records and replays stay complete.
  Collaborative editing follows: clients send edit inputs, the server steps (D12.3), and the editor
  server is the game server.

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
- **S2.3 Identity and placement are separate.** An entity's *key* is its identity: a 64-bit value
  from a per-world monotonic counter, never reused (48 bits last nine thousand years at a thousand
  spawns per second), used in references, files and on the network alike. There are no generations,
  because a key is never reissued; a reference to a dead entity resolves to "dead", never to another
  entity. An entity's *slot* is its placement: the row its components occupy in the paged tables
  (D4.4), assigned by a deterministic rule from state (its cell, D3.2b, then key order), dense per
  cell, and repacked at the tick boundary by the step that already performs cell transfer and death,
  so within a tick slots are stable and nothing persistent holds one. A slot is derived state in the
  sense of S4.5: never saved, never replicated, rebuilt on load, and a client assigns its own slots,
  so its tables hold its interest set densely instead of mirroring the server's key space. The
  key-to-slot map is one more compacted paged table (S4.4a) keyed by key; a dead key's entry says
  dead, and a page whose keys are all dead is freed, so the map tracks live entities, not total
  spawns. The cost is one extra load when a reference is followed; bulk iteration walks slots and
  never resolves a key. There is no per-entity random `UID`; `UID` remains the identity of things
  authored independently and merged without a central allocator: assets, prefabs, component and
  message types, services. The key counter is part of the saved world, prefab-internal references
  are remapped at instantiation, and in multiplayer the server allocates keys while clients use a
  reserved range for predicted entities until confirmed. Rejected on the way here: index:generation
  keys with reuse, which needed FIFO queues with minimum pools against generation wrap and then
  per-class pools against fragmentation, a stack of patches for using one number as both identity
  and placement.
- **P2.4 Diffs are first-class.** Every versioned structure can enumerate changes between two
  snapshots in time proportional to the change (`visitDiff` today). Replication, render upload,
  save deltas and editor change tracking are all diff consumers.
- **P2.5 Retention is explicit.** Snapshots stay alive while referenced; the cost is proportional
  to retained changes. Rollback netcode and editor undo are "keep N snapshots and replay".
- **P2.6 Measure before building on it.** A benchmark that updates 100k components per tick through
  the storage layer, with snapshot retention of 1, 4 and 64 ticks, is the gate for the ECS design.
  The storage half exists as `src/sandbox/bench_v8g.cpp` (target `bench-v8g`); its results are in
  "P2.6 results" at the end of this section. Still to measure there: slot-indexed pages behind a
  key-to-slot map, with repacking under spawn/despawn churn (D4.4), and compacted pages at several
  densities (S4.4a). No messaging benchmark exists yet.
- **P2.7 Immutability is enforced, not promised.** The one real V8G consumer broke the contract
  until 2026-10-28: `editBlock` in `src/voxen/land/land_service.cpp` mutated a published chunk in
  place and then inserted a null value for its key. Published values must be `const` all the way
  down (snapshot access returns `const` only; mutation goes through a copy-on-write handle that the
  storage hands out for the current tick). Debug builds can additionally verify that no snapshot
  older than the current tick is written.
- **D2.8 Grids are the third storage shape.** V8G has tables keyed by entity index (D4.4) and blobs
  (D4.3a); neither holds a voxel world's block layers (hundreds of millions of blocks, copied whole
  per chunk edit today: 64 KiB expanded and recompressed per block, 50-100 ms for a thousand edits
  per tick) or a city's diffusion fields (twelve grids of a million cells; a one-cell zoning stroke
  as a blob copies 2 MiB and re-uploads it per frame), and three plugins would otherwise build the
  same tile machinery. The paged table of D4.4 is therefore generic over a 48-64-bit integer key:
  the entity index is one key, the Morton code of a grid coordinate at a declared tile size is
  another. A grid is a typed element from the schema in an N-dimensional space aligned to the frame
  grid (D4.6), with the tile as the page (an 8^3 sub-chunk of block ids is 1 KiB, so an edit copies
  and retains 1 KiB, not a chunk), copy-on-write, diffed (P2.4), journaled (D16.2) and replicated
  per tile exactly like component pages, from the same page allocator (D17.2). Two page leaves join
  S4.4a's menu for grid data: *uniform* (one value for the whole tile, 60-80% of a voxel world's
  sub-chunks) and *palette* (a per-tile table of distinct values plus 2-8-bit indices, Minecraft's
  section palette: 256 B instead of 1 KiB at sixteen states). A grid is a component field on a
  well-known entity (P4.8) whose system is its single writer (P1.13); anyone samples it freely
  (S1.2), stencils included. A scale-n cell's region is a contiguous Morton range, so bulk systems
  iterate it as a page range (P3.2) with no allocator help. The voxel chunk store is the first
  instance (P4.7), a heightmap terrain the second, a city's fields the third; `land::ChunkKey` keeps
  its fields but orders as a Morton code. The P2.6 benchmark gains a Morton-keyed configuration with
  a stencil write pattern.
- **D2.9 A write that changes nothing is not a write.** S2.2 copies a page on first write per tick,
  not on first change, and a system that recomputes whole pages (a cellular automaton, a flow solver
  that writes all 200k results every solve) would copy and retain everything it visits: a fluid
  system over 10k resident chunks touching 640k sub-chunk pages is 640 MiB per tick and 38 GB/s,
  which no retention count survives, for a steady state where nothing moved. Such systems compute
  into per-tick scratch (D17.2) and *compare-and-publish* a page only if it differs from the
  snapshot's; the storage API exposes exactly that, and the invariant checker (P18.4) counts a
  published page equal to its predecessor as a finding, since a no-op write inflates diffs,
  retention, replication and journals alike. They also keep an *active set* (an index, S4.5: a
  bitmap over pages maintained from the previous tick's diff) and visit only it, so the cost of a
  layer is its active fraction (0.5-5% for a flood against a steady state: 3-32 MiB per tick) times
  page size times retention, budgeted per layer (P17.1). P1.7's per-entity phase offset is opt-out
  for such systems: neighbouring pages must step on the same tick.

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
- **P3.2 Locality comes from data layout, not thread pinning.** The unit of scheduling is a *cell*:
  a spatial or logical group (chunk, region, explicitly declared group) whose entities are updated
  by one task. Messages between entities of one cell stay local; cells are the natural unit for work
  stealing, for save/stream granularity and for replication interest. Sticky thread assignment and
  dynamic communication-graph clustering are deferred until a profile of real workloads shows cells
  are not enough. The two compute models (P1.6) are scheduled at two granularities: reactive entity
  logic per cell (one task per cell per phase), bulk systems per page range of the tables they
  iterate (D4.4). Slot assignment (S2.3) packs a cell's entities into one range so the two coincide,
  which is the real reason for that packing; a system iterating a table never knows which cells it
  crosses.
- **D3.2a Cells are ownership, frames are coordinates.** A large-world frame (D4.6) is a fixed,
  regular grid cell of the coordinate system; a sim cell is a unit of ownership: scheduling,
  persistence (D16.2), replication interest (D12.2). They are not the same thing but spatial
  cells align to the frame grid: a spatial cell is a frame-aligned region at some scale (as
  `land::ChunkKey` with `scale_log2` already is), so a spatial entity's cell is derived from its
  frame coordinate and the cell scale. Cells are variable-size (scale per region, coarser where
  nothing happens), created and removed programmatically (a logical cell for a factory network
  or a vehicle interior; a spatial cell split when it gets too busy), and may overlap in space (a
  moving vehicle's cell crosses terrain cells). An entity belongs to exactly one cell: single
  ownership is what keeps persistence and replication unambiguous, and any other grouping
  (tags, "all machines of this network") is an index (S4.5), not membership. Moving between
  cells is an ownership transfer at the tick boundary, driven by state (position, an explicit
  reparent), so it is deterministic (S1.4). Cell boundaries are visible to nothing but the
  scheduler, the saver and the replicator: entity logic never sees them. A child in the
  hierarchy (S4.3) is in its root ancestor's cell and moves with it. A world may hold several
  *spaces* (disjoint coordinate systems: a city, a resort, another dimension) at once as
  separate scene roots (S4.2): a space is a property of the cell, not of the position, so
  positions stay frame plus offset (D4.6) and nothing pays for spaces it does not use;
  travel between spaces is a cell transfer like any other.
- **D3.2b What a cell holds, and how an entity gets one.** A cell owns two things: the entities
  whose slots lie in its slot range (S2.3, D4.4), and, for every grid (D2.8), the Morton range of
  its region at its scale. Both are contiguous ranges, so a cell's save, drop, diff, replication and
  ownership transfer are range operations with no per-plugin hook: a terrain plugin declares its
  layers as grids and gets persistence (D16.2), interest (D12.2) and scheduling (P3.2) for them
  without the core knowing what a block is (P4.7). The frame unit (a chunk, scale 0) is the granule
  of generation, LOD and meshing and has no ownership role; a cell at scale n holds 8^n of them.
  Which cell an entity is in is a declared *cell rule* per archetype: `spatial` (from the frame
  coordinate), `parent` (from the root ancestor, S4.3), `follow(field)` (the cell of the entity a
  reference field names: a citizen is in the cell of its home and moves cell when it moves house,
  which is state, so the transfer is deterministic), or `shard(n)` (shards of n entities for
  populations with no anchor, split and merged by the placement step). Every entity has exactly one
  cell at every moment, including creation: there is no cell-less entity, because placement needs
  the cell and a class of entities no saver or replicator sees is a class of bugs. Entities in limbo
  (a logged-off player's belongings, something in transit between spaces) live in a logical cell
  that persists and replicates like any other. The cell lives in the entity header (D4.4), written
  only by the ownership-transfer step at the tick boundary, which re-slots a migrant into its new
  cell's range, so locality follows ownership and a migrant keeps nothing but its key.
- **D3.2c Cells are split for scheduling and grouped for everything else.** A single arena or
  campaign section is one spatial cell, which makes its reactive logic (P3.2: one task per cell per
  phase) single-threaded: forty AI and twenty scripted entities at 50-150 us each are 3-9 ms in one
  task on a console core. A project therefore declares logical cells for scheduling (players,
  projectiles, each squad, the director; about twice the core count), grouped by message graph
  rather than space, and declares a *cell group* that interest (D12.2) and persistence (D16.2) treat
  as one unit, so the split costs nothing on the wire or on disk. The one-tick message latency
  between cells stays. Deactivation uses the same machinery and no dedicated flag: a *dormant* cell
  is simply not scheduled (zero cost, pages untouched; eviction through D16.3 is the same thing one
  step further), and deactivating an individual inside an active cell is removing the tag its
  systems iterate (a zero-size component is a presence bit, D4.4) or, for a reactive entity, the
  normal state of waiting for a message (P1.7). Unity-style subtree deactivation falls out at the
  cell level, since children are in the root ancestor's cell.
- **P3.3 Task system stays generic.** `TaskService` (counters, dynamic graphs, continuations,
  coroutines) is the only thread pool. Sim scheduling, asset streaming, render preparation and
  tools all launch into it; no subsystem owns threads except the ones with external constraints
  (OS main thread, GPU submission, audio callback).
- **P3.3a Presentation has priority.** The one pool (P3.3) runs tasks in priority classes:
  presentation (render preparation, UI, audio feeding) above simulation above streaming and tools,
  and a presentation task never queues behind a sim phase's fan-out. Whether that is achieved by
  static reservation of a worker or by adaptive quality-of-service is an implementation choice: it
  never enters the simulation (P3.1), so it is not a design question. The tick barrier (P3.4) may
  slip behind presentation work; a frame never slips behind the tick, which D10.4 protects in only
  one direction. A 144 Hz frame has 6.9 ms, and a 2 ms sim phase in front of a frame task is a
  dropped frame.
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
- **D4.1b Bulk tables migrate by column, documents by DOM.** The DOM round trip of D4.1a is the path
  for documents and small tables; a component table of 10^6 rows never becomes a tree of boxed
  values (a city save of 3x10^7 field nodes at 64-100 B each is 2-3 GB of transient memory and tens
  of seconds, on every update that touches a layout). Schema codegen (P7.7) emits, per component
  type and per version step, a page-level converter that runs over the table in parallel on load; a
  step the generator cannot derive (a field split, a unit change) is a hand-written function with
  the same signature, and the chain of steps is the table's P7.6 converter. The regression corpus of
  P7.6 includes one full-size save per schema version, and the load time of that save is a tracked
  benchmark (P19.3).
- **S4.2 Scene is a snapshot, prefab is a document.** A scene file is a serialized snapshot (set
  of entities with components). A prefab is a document whose instantiation is a DOM merge
  (base document + instance overrides), so prefab overrides, asset-meta inheritance and settings
  layering are one mechanism. Nested scenes are prefabs that happen to be large. A world has any
  number of scene roots resident at once, each a space of its own (D3.2a).
- **S4.3 Hierarchy is data.** `Parent` is a component (`parent key, order`) and the single
  source of truth. `Children` is not stored: it is a reverse index of `Parent` maintained from
  diffs (S4.5), which removes both the variable-length component and the two-sources consistency
  problem. `Transform` stays purely spatial and does not absorb the parent link: non-spatial
  entities (a machine's logic holder, groups, UI nodes) take part in the hierarchy for ownership
  and active/disabled propagation without carrying a transform, and a spatial entity resolves
  its world transform against its nearest spatial ancestor. Propagation (transforms, active
  state) is a system phase (S1.5). Streaming, LOD and replication ownership attach to cells
  (P3.2), not to the hierarchy. `Parent` means exactly "moves with and dies with" (transform
  attachment and lifetime, including the cell, D3.2a): a character riding a vehicle, an item
  in an inventory. Every other relation (faction membership, employment, property ownership)
  is a reference component (`Member { faction }`) with a reverse index (S4.5); a project defines
  as many such hierarchies as it needs on the same machinery, and none of them is the engine's.
- **D4.3a Variable-length data in components.** Not every component is a fixed-layout POD
  (inventories, names, scripted tables). The schema distinguishes *inline* fields (fixed layout,
  in the page) from *blob* fields: a handle to an immutable, reference-counted buffer copied on
  write, which the Immutable/Shared storage policies of V8G already model. Blobs keep components
  trivially copyable and snapshot-safe (a copy is a reference bump) at the price of an
  indirection; the rule that a component never points into another entity or component still
  holds.
- **D4.4 One paged table per component type, indexed by slot.** With placement separated from
  identity (S2.3) the archetype-vs-sparse-set choice mostly dissolves: every component type is a
  paged array indexed directly by slot (a radix tree of pages over the slot space, presence bitmap
  per page), so a lookup by slot is two loads (three from a key), iteration walks the pages of one
  type and joins other types by slot, and pages are also the copy-on-write unit (S2.2). Locality,
  which is what archetypes buy, comes from slot assignment: a cell's entities occupy a dense slot
  range, ordered by archetype within it, so the pages of the types they share are dense and iterate
  like an archetype table, and a cell transfer or a prefab instantiation is a slot assignment in the
  target cell. A schema-declared archetype is then a placement hint plus a guarantee that its types
  are always attached together; a rarely attached component is a type whose pages are mostly absent.
  The explicit entity-to-components mapping is the entity header table (one more slot-indexed table,
  always present): key, cell and the set of attached types as a bitset or, past a type count worth
  measuring, a small sorted list or archetype id. Adding or removing a component is a header diff
  plus a page write; nothing moves except at the boundary repack. To validate by extending the P2.6
  benchmark with slot-indexed pages behind a key-to-slot map, repacking under spawn/despawn churn,
  next to the hash trie and the flat paged table it already measures.
- **S4.4a Sparse types do not pay for full pages.** A rare component (a trait of one mob type) on
  scattered slots would cost a whole page per resident entry (64x for 64-entry pages). The escape
  hatches change the page leaf, never the key, the directory or the API, and are chosen per type.
  Default: *compacted pages*, a 64-bit presence mask plus an array of only the present entries,
  entry = `popcount(mask & below)`, the node layout of `CompressedChunkStorage` and of the hash
  trie; a full page has an all-ones mask and costs one popcount over a dense page, memory is
  proportional to present entries at any density, and the shifting insert is already paid by the
  copy-on-write page copy (handles into a mutable page are index-based, never pointers). Per-type
  page size (down to single entries) and per-type *sparse* storage (a map keyed by slot: the hash
  trie, which measured fine at low update rates) are the schema-level policies for what compaction
  does not cover, such as very large components. Switching a type's representation is a whole-table
  rebuild at a tick boundary; older snapshots keep their own tables. Index sub-ranges per subtype
  are not needed: placement is repacked per cell (S2.3), so spawn/despawn churn never fragments a
  table.
- **S4.5 Indices are views of a snapshot.** Primary storage (V8G) is the only authoritative
  state; anything that answers a query faster is an index in the database sense: a spatial hash
  or BVH for "what is near me", the `Children` reverse index, name and tag lookups, "machines
  waiting for power" lists. An index is maintained by a system from diffs (P2.4) in a phase after
  its sources' writers, is published with the snapshot it describes (immutable and versioned
  like a component page) and is never saved or replicated: losing it is a rebuild, never data
  loss. An index is copy-on-write when it is large and rebuilt per tick when it is small; which
  one is a per-index decision measured against the P0.5 workload.
- **S4.5a Indices may lag or be lazy, by declaration.** S4.5's "published with the snapshot it
  describes" has two declared exceptions. An index whose rebuild does not fit a tick (a routing
  hierarchy over 80k road segments is about a second and has no cheap incremental form) is rebuilt
  over a fixed number of ticks named in its schema, in equal state-derived slices, never "as much as
  fits" (S1.4); the published index records the tick it describes, the previous version is served
  until the new one completes, and consumers must be correct against the live snapshot (a path found
  on graph k is validated segment by segment against snapshot i and repaired where it crosses a dead
  key). This is the CPU form of D10.4's pipelined results. An index whose sources are immutable
  snapshots may be *lazy*: filled on first query, immutable once filled, retained with its snapshot
  (the rewindable hit-query index of D14.6 poses three candidates per shot instead of 64 skeletons
  per tick).
- **D4.6 Large-world coordinates from day one.** Retrofitting is the lesson of Unreal's LWC, so
  the sim transform is split from the start: an integer frame (cell coordinate, as
  `land::ChunkKey` already is) plus a local offset, and only the combined position crosses
  cells, saves or the network. Recommendation: `int32` frame on the chunk grid plus `float`
  local offset, rebased on cell crossing, which keeps a position 16 bytes and SIMD-friendly;
  `double` only where a solver integrates over many ticks (physics), and strict-tier logic on
  integer block coordinates anyway (S1.4). Render has no choice: camera-relative translation
  computed once per frame on the CPU (frame difference in integers, remainder in `float`), and
  `float` in everything that reaches a shader.
- **P4.7 The core is geometry-agnostic.** The engine core knows cells, frames, components and
  indices; it does not know blocks, heightmaps or meshes. Voxel land is the terrain plugin that
  happened to exist when this draft started (P6.3), a heightmap terrain would be another, and a
  project with neither (a space game) loads none. D14.3 describes one collision shape, not a
  core assumption.
- **P4.8 Global and per-region state are components on well-known entities.** Weather of a
  location, the price table of a town, the rules of a world: components on the location, town
  and world entities, written by their systems (P1.13) and read freely by everyone (S1.2).
  There is no separate singleton or "resource" concept; "which location am I in" is answered by
  the cell (D3.2a), which knows its location entity.

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
- **D5.6 Behaviour mods replace systems and insert phases.** A native plugin cannot patch another
  plugin's code (the Harmony patches a Cities: Skylines workshop runs on have no equivalent), and
  P1.13 means a mod cannot "also" adjust a component: it must be the writer. The base game is
  therefore moddable at three seams and factored for them. *Replacement*: a plugin registers a
  system under an existing system's UID and takes over its component ownership; the base one is not
  scheduled, and two plugins claiming one UID is a load-order conflict reported at startup and
  resolved by the plugin layer order (P7.2), never silently. *Insertion*: a plugin declares a phase
  with "after X, before Y" edges in the phase graph (S1.5) and owns components of its own.
  *Parameters*: every tunable of a base system is a field of a schema document layered like settings
  (D7.8), so a "realistic population" mod is a document layer, not code. Base-game systems are
  consequently small (one responsibility, one or two owned types), because the system is the unit of
  replacement, and a save records the mod set in force (D16.5).

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
- **D6.4 Capabilities of the two buses.** They do not share a capability list because they answer
  different questions: the control plane asks "who serves this request", the data plane "what does
  entity X see at tick `i+1`". Control plane (P6.2): unicast *request/reply* with a correlation id,
  asynchronous, with timeout and cancellation; *request/stream*, one request and many replies until
  an end marker (log tails, profiler samples, asset build progress, script debugger events, D22.4),
  which the tool protocol needs on day one; *publish/subscribe* by type or named topic as the only
  multicast form, broadcast being a well-known topic, with no "send to this set of addresses"
  primitive. Payloads are schema-declared types with a DOM serialization (P7.1), passed as typed
  structs in-process; a pure signal is a type with no fields, and one declared opaque-bytes type
  covers crash dumps and asset bytes. Ordering is FIFO per sender-receiver pair and nothing more.
  Data plane (S6.1): *unicast to an entity key* is the only transport primitive, delivered at `i+1`
  in (sender key, sequence) order (S1.4), dropped when the key is dead, with an opt-in bounce to the
  sender; *timers* are unicasts to self stamped with a future tick, stored in the outbox like any
  message; request/reply is two unicasts plus a state machine (S1.8), with "reply to" free since
  every message carries sender and sequence; *no multicast and no broadcast*, because reads are free
  (S1.2): anything many entities care about is written once as state, an event log component on a
  well-known entity (P4.8) or on the cell, and scanned next tick, since expanding a multicast at
  send time is an O(N) write and expanding it at delivery would make an index (S4.5) part of message
  semantics. Payloads are schema-declared fixed-layout types generated like components, with D4.3a
  for variable-length parts; no DOM on the sim path. Each message type declares a *delivery mode*:
  ordered (every message, in order) or coalescing (several of one type to one receiver in one tick
  collapse into one), the latter for wake-up signals (P1.7) and for bounding outbox growth when
  everyone pokes the power grid. Entity messages never cross the network (D12.3) and cross cells by
  outbox merge at the tick barrier.

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
  session timeline (render, audio), or *sim input* (S1.12), which turns a change into a tick input
  instead of a direct read; cheat and replicated flags cover multiplayer. There is no separate
  cvar system: a cvar is a settings field with a console path.
- **D7.9 Registries map authored UIDs to per-world dense ids.** Block ids live in grid tiles by the
  hundred million and item ids in every stack, so they must be small dense integers, while the types
  are authored by hundreds of mods and identified by UID (S2.3); nothing mapped one to the other,
  and the mapping is world state. A registry is a schema-declared table from type UID (block type
  and state, item, recipe, biome: anything stored by id in bulk data) to a dense integer allocated
  per world on first use and saved with it (P16.1), like entity keys. Ids are never reassigned (the
  id-shift corruption of early modded Minecraft is the lesson); a UID whose mod is absent on load
  maps to a schema-declared placeholder and keeps its id, so returning the mod restores the content.
  The same table drives the generated protocol (D12.4): the handshake exchanges registries, not only
  the schema hash, and the client uses the server's ids. Bulk data stores registry ids (through
  palettes, D2.8), never UIDs; UIDs appear only in documents.

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
- **D8.4a Hot reload is streaming along the version axis.** A changed asset (editor edit, mod
  update, a locale switch, P20.1) is one more residency change, like a new LOD level or a
  partially resident region: the handle stays, a new version of the contents becomes resident,
  consumers observe "version changed" through the same path they observe "level N arrived",
  and the old version is released when no consumer references it. Consequences: hot reload is
  a shipping feature, not a development one; no consumer caches a raw pointer into asset
  contents across the point where it could observe a version change; GPU residency is the same
  mechanism one level down.
- **P8.5 Immutable assets, mutable documents.** Assets never change at runtime; documents (scenes,
  saves, profiles) do. The storage layer treats them differently (P7.4, design-drafts "Data and
  storage").

## 9. Tools, editor, agents

- **P9.1 Editor = player + plugins, and everything ships.** One binary; editor features are
  plugins using public APIs, and nothing is tool-only: what the editor can do, the game can do
  if the project wants it to (an in-game build mode can be `edit()` of S9.4 on a paused world).
  The development/shipping switch (P11.3) strips diagnostics rather than capabilities.
- **P9.2 Headless first.** The engine runs without a window or GPU (the core/sim/gfx library split
  from design-drafts). Tests, servers, asset cooking and agents all use the headless mode.
- **P9.3 Everything callable is a command.** Editor operations are messages (P6.2) with text
  encoding; the GUI is one client of them, an LLM agent another, a test another. Visual feedback
  is a command too (render a view to an image).
- **S9.4 Undo is snapshots.** Editor undo/redo retains snapshots (P2.5) instead of command
  inversion. An edited world is stepped by `edit(state, operations) → state'`, which is the step
  function of S1.15 configured for the editor timeline: zero simulated time, the editing phase set,
  editor operations as input records, one snapshot per operation and full retention. Both
  configurations consume and produce the same snapshot type, so play-in-editor is "start ticking
  from this snapshot" and stop is "discard back to the edit snapshot". Undo depth is a retention
  count, redo is the retained forward chain until a new edit replaces it.
- **S9.4a Document undo is the same shape.** Editor plugins also edit documents (P7.1: a dialogue
  graph, a prefab, a settings layer) rather than worlds, and S9.4 does nothing for those: their
  changes reach a running playtest by asset build (P8.3) and hot reload (D8.4a). Documents are
  persistent values too: each operation yields a new document version, undo and redo are retention
  of versions (P2.5), and the asset build consumes document versions incrementally per record (a
  UID-keyed line, not a 10 MiB table), so a writer's edit-test loop is an in-place relocation
  (S1.8), not a rebuild and a restart. A world edit and a document edit are two undo stacks the
  editor keeps apart. Starting a playtest from a point in content is `edit()` applying a scenario
  document (P19.1) to a paused world, so authors' test presets and the regression suite are the same
  files.

## 10. Rendering and UI

- **P10.1 Render reads snapshots.** The renderer consumes the latest published snapshot and
  diffs (P2.4) to update GPU-side state; it never touches mutable simulation data and runs on its
  own timeline.
- **D10.1a Presentation predicts and reconciles, networking or not.** A 144 Hz presentation of
  a 60 Hz sim that only interpolates between snapshots adds a tick of latency and quantizes
  input to the tick rate; for camera rotation that is visible jag and lag. So the presentation
  timeline owns a *predicted state*: each frame it starts from the latest snapshot, applies the
  input records not yet consumed by the sim (P15.1) and the sub-tick remainder, and renders
  that; when the next snapshot arrives the pending set shrinks and the prediction is rebuilt
  from it. This is the structure of client prediction (D12.3) with the local sim thread in the
  role of the server, one tick of latency instead of a round trip, so the scope rule is shared:
  own camera and avatar are predicted, everything else is interpolated. The predict function
  is project code (a plugin, like the sim), runs on the presentation timeline, is tolerant-tier
  by nature and feeds nothing back. The pure camera case has a cheaper special form: the camera
  orientation is presentation state that the sim receives as an input field, so mouse look is
  never tick-rate-limited at all.
- **D10.1b Prediction emits presentation events, and discontinuities are marked.** Own gunfire
  through the sim path is up to a tick of wait plus publish plus the audio buffer, 25-35 ms after
  the click on top of the 12-30 ms visual chain, and action-to-sound drift is perceptible past 20-30
  ms. The predict function already knows the shot happened, so it produces the events its predicted
  actions imply (a shot, a footstep, a weapon switch), tagged with the input sequence that caused
  them, and the presentation consumes them within the frame; the sim's own event for the same
  sequence is deduplicated on arrival, and a predicted event the sim never confirms is cancelled by
  a project rule (stop the sound, or let it play). "Feeds nothing back" still holds: events go to
  the presentation, never to the sim. Separately, respawn, teleport, camera cuts, cell or space
  transfer and corrective snaps must not be interpolated or predicted across: a transform change
  carries an *epoch* (a counter the writer bumps on a discontinuity), the interpolator and the
  predict function treat an epoch change as a cut, and the replication encoder transmits it.
  Smoothing of small corrections is presentation policy on top, never in the sim.
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
  enter the simulation pipelined, N ticks later, as inputs (the pattern of messages and S1.12),
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
  subscribes to a set of cells (around its avatars, plus whatever the game pins: a remote camera, an
  owned base) with hysteresis on the boundary, and receives per-cell diffs. By default a cell is in
  or out; per-entity relevance inside a cell is an opt-in filter (D12.2a). Replication priority and
  bandwidth budgeting are per cell per client (nearest first, stale-longest first), so the P0.5
  world with thousands of machines costs a client only the cells it looks at.
- **D12.2a Relevance within a cell is an opt-in project filter.** In an arena every cell is in every
  client's interest set, so cell-only interest ships all 63 opponents to every client every tick,
  and on a platform where everything ships (P9.1) and mods are peers (P5.5), a client plugin that
  draws what it receives through walls is a fifty-line wallhack; the server's only defence is not
  sending. A project may therefore declare a *replication filter*: a per-client index (S4.5) written
  by a server system from state (potential visibility with a latency margin, team, spectator
  rights), applied by the encoder so a filtered entity is omitted from the client's diff rather than
  frozen. It costs a table lookup per pair (0.2 ms per tick for 64 players with a precomputed
  visibility set and periodic ray refinement) and is deterministic from state, so a server replay
  reproduces what each client was sent. A project that declares no filter pays nothing: the
  encoder's default path is D12.2's cell in or out.
- **D12.3 Clients send inputs, never state.** The wire carries the client's input records
  (P15.1) up and cell diffs down. Prediction scope is the client's own avatar and what it
  directly manipulates in the same tick (strict-tier interactions it initiates: a door, a
  pickup); everything else is interpolated from server snapshots and never predicted.
  Reconciliation is "retain snapshots, re-tick from the last confirmed one with the pending
  inputs" (P2.5), which bounds the retention count by round-trip time in ticks.
- **D12.3a Prediction re-ticks a cell set, not the world.** A correction replays RTT-in-ticks of
  pending input (4 at 60 Hz, 8 at 128 Hz for 60 ms), and a whole-arena tick on a console core (phase
  fan-out and barriers, a solver step for 250 bodies, a solver restore) is about 1 ms, so one
  correction at 128 Hz is 8 ms, the entire CPU budget of a 120 fps frame. Predicted entities live in
  cells the client marks as *predicted*; reconciliation re-ticks only those cells with every other
  cell held at the server's confirmed state as read-only context (tens of microseconds per re-tick),
  and the client's steady-state tick runs the same reduced set: interpolated cells are never ticked
  on a client, and a sandbox client never runs `tick()` over its interest set at all (it would run
  automata and machines over a partial world and diverge). Systems therefore step per cell (P3.2
  already schedules reactive logic that way; bulk systems take a cell filter) and the solver is
  instanced per cell (D14.5).
- **D12.4 The protocol is generated.** Diff encoding, input records and the session handshake
  come from the schema (P7.7) with a protocol version derived from the schema hash; mismatched
  versions refuse to connect rather than negotiate. One unreliable-ordered channel with
  acknowledged baselines for cell diffs, one reliable channel for session control. Transport
  (UDP, a relay, in-process loopback for tests) sits below both.
- **D12.4a Wire encoding is a schema policy, and the send rate is not the tick rate.** A raw
  component diff of a 64-player arena as D12.4 reads today (whole dirty components, 156 B per player
  per tick) is 5-10 Mbps per client; a cell of a thousand machines whose progress counters change
  every tick differs in a thousand components from any baseline forever. The schema (P7.7) therefore
  marks every component field *replicated* (sent in cell diffs, with declared quantization: position
  to 1/64 m, angles to 16 bits, and delta against the client's acknowledged baseline at field
  granularity), *derived* (never sent: the client computes it from other fields, from a start tick
  and a rate, or from a client-simulated system, D12.6) or *rate-limited* (at most every N ticks,
  per type: players every send, ragdolls and debris at 10 Hz or presentation-only). A field with no
  policy defaults to replicated, so forgetting one costs bandwidth, not correctness. The per-client
  send rate divides the tick rate (128 Hz tick, 64 Hz send); encoding is one task per client at the
  send rate (3-6 ms serial for 64 clients otherwise); the baseline window bounds server retention at
  about a second of sends, and a client further behind receives a full cell state. The P0.5 network
  scenario measures a 64-player arena against 1 Mbps and a cell of a thousand machines against 100
  KB/s per client.
- **D12.5 Mods on the server are the trust model.** Simulation mods run on the server; clients
  must have the same mod set (checked in the handshake) for prediction and presentation, and a
  mismatch is a refusal, not a download. Native plugins are not sandboxed (P5.5): installing a
  mod is trusting its author, as on every other PC platform.
- **D12.5a Anti-cheat scope.** The engine's contribution is exactly: authority (D12.3: the server
  accepts inputs, never state); input validation hooks (edge rate, angular velocity, sub-tick stamps
  monotonic within the tick, the rewind window of P1.1a bounded by *measured* round-trip time plus
  jitter and clamped, never by a claimed timestamp); relevance filtering (D12.2a) so a client holds
  as little as it needs; and replays as evidence (P16.4: the server's input log is about 37 MiB per
  ten-minute 64-player match and re-ticks exactly for strict-tier hit registration, D14.6). It does
  not include client integrity: the mod-set handshake (D12.5) is a compatibility check, not
  attestation; a shipped client contains the editor plugins (P9.1) and loads native plugins (P5.5),
  so memory and plugin tampering are detected, if at all, by an imported anti-cheat (P0.3) whose
  attestation the server may require in the handshake. Aim, trigger and wall assistance that submit
  valid inputs are detected statistically from replays, outside the engine. Griefing in a sandbox is
  the same shape: every edit is a recorded input validated by the avatar's own logic (P15.1a), so
  the audit log is free.
- **D12.6 Replication has three forms: diffs, client simulation, lockstep.** D12.2 assumes a bounded
  viewpoint, and two workloads break it. A voxel world's per-tick change is dominated by derived
  layers: a fluid front of 10k blocks per tick inside one client's interest set is 2.4 MB/s, ten
  times an internet budget, and sending at 10 Hz does not help because the set of changed blocks
  differs every tick. A god-view client watches every cell: full replication of a city's 10^5 agents
  is a hundred times a consumer uplink, and per-cell priority only decides which cells go stale. So,
  per project: a strict-tier system (S1.4) that reads only grids and components the client holds may
  be declared *client-simulated*: its outputs are never replicated, the client runs it over its
  interest set plus a declared halo, and the server sends per cell every N ticks a hash of the layer
  so the client detects divergence and requests the cell's pages (the baseline path of D12.4).
  Fluids and light are the first two; this is not prediction of player actions but computation of
  consequences the client has all the inputs for. *Replication LOD*: a cell outside the near set
  replicates a schema-declared subset of types at a declared period per distance band (buildings and
  per-segment congestion every 60 ticks, no vehicles) and the presentation synthesizes the rest.
  *Lockstep* (S12.1) for a fully strict sim (S1.4a) with global interest: the wire carries inputs
  only, every client runs the whole world, affordable precisely when the working set is the world
  anyway (P17.3 does not apply). A sandbox would take client-simulated layers, a god-view game
  lockstep or replication LOD depending on how strict its sim is (S1.4a), and a game with both a
  near view and a map replication LOD; none of these is the engine's call.

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
- **D14.5 Avatar movement need not be solver work, and the solver is instanced per cell.** Avatar
  movement (walk, slide, mantle, riding) can be a kinematic controller system in the sense of P14.4
  rather than tolerant physics: integration in fixed point or on an integer grid, with shape casts
  against static collision through the solver's query interface only, which makes prediction
  bit-exact on every platform so crossplay does not mispredict every tick. Whether a project wants
  that is its own tier decision (S1.4a); the engine's part is that the solver's query interface is
  usable without stepping the solver, so the choice exists. Driven vehicles stay in the solver and
  the price is known: predicting one means solver save/restore and re-step per correction (50-100
  KiB and about 0.2 ms per restore for an arena). The solver is instanced per cell (D14.3 already
  allows per-region instances) so that restore and step are cell-local (D12.3a).
- **D14.6 Hit queries have their own rewindable index, and the sim poses what gameplay depends on.**
  Lag-compensated hit registration queries targets at tick `k < i` (P1.1a), but the solver's world
  (P14.1) exists at tick `i` only, and D22.3 puts pose evaluation on the presentation while an
  animated hitbox test needs bone transforms in the sim at tick `k`. So: queries against the past
  use a separate per-snapshot index (S4.5, lazy per S4.5a) of the rewindable shapes, player capsules
  and lazily posed hitboxes keyed by entity and tick, about 5 KiB for 64 players, retained with its
  snapshot; statics are queried at tick `i` through the solver, the accepted approximation;
  projectiles sweep this index on each tick of flight at the shooter's rewind tick. Animation
  evaluation then exists on both timelines: the presentation evaluates full skeletons per frame
  (D22.3), and the sim evaluates a reduced *gameplay skeleton* (hitbox bones, hand sockets, root
  motion) from `AnimState` on demand, posing only the candidates a capsule pre-test leaves (three
  per shot, not 64 per tick: 0.07 ms instead of 0.6 ms per tick at 128 Hz). The result is quantized
  to a fixed-point pose so hit registration is strict-tier and a replay reproduces every hit
  bit-for-bit (P16.4, D12.5a); blend weights stay float and tolerant.

## 15. Input

- **P15.1 Devices never reach the simulation.** Raw device events (SDL today) are mapped on the
  presentation side to *actions*: project-declared in the schema, rebindable through the
  settings document (D7.8). Per tick the mapping produces an *input record* (action states and
  edges, tick-stamped, per player); that record is the `inputs[i]` of P1.1 and the only input
  the simulation ever sees. Immediate-mode UI (S10.2) consumes events before the mapping,
  so focus and capture are a presentation concern.
- **P15.1a Commands are input, and edges carry their time.** "Action states and edges" is not the
  whole record; three of the four stress-test games asked for the rest. *Commands*: discrete,
  payload-carrying, schema-declared (P7.7), delivered exactly once at the tick they are stamped
  with: a dialogue choice by node and option id, place block B at P on face F, move N items from
  slot a of X to slot b of Y, press button K of machine M, an editor operation (S1.15). The UI
  (S10.2) emits them after resolving its own hit testing, so a record never depends on layout,
  locale or resolution and a replay re-derives the same choices from the same snapshots. A command
  is a message (S1.3) whose sender is the player and whose target is the player's avatar entity,
  validated there by the avatar's own logic (P1.13: reach, inventory, ownership, rate) before any
  effect is forwarded, flagged *predictable* individually (block placement is, a trade is not), and
  recorded with the rest of the record (P15.2); nothing reaches state except through this path,
  which is what makes it the audit log (D12.5a). The same channel carries character-creation values
  and the entropy of a deliberately re-rollable check. *Timing*: each edge is stamped with its
  sub-tick fraction (16 bits) and the action's payload at that instant (for fire: the view angles at
  the click, 2x16 bits; at 30 deg/s of flick a 60 Hz tick is 0.5 deg, 87 cm at 100 m), and the
  record carries the presentation state the player acted on (the server tick and fraction the client
  interpolated others at, D10.1a). Systems that care resolve edges at their fraction: a shot is
  rewound to the click, a round fired at fraction `f` advances `(1-f)*dt` in its spawn tick, a
  high-rate weapon emits several rounds per tick each at its own offset. The per-tick camera field
  of D10.1a is the end-of-tick orientation and is never used for hit resolution. The server stamps a
  record with the tick it consumes it in, never the client's tick, and the replay stores the
  server's stamp.
- **P15.2 Input records are the replay format.** Recording the records (plus the initial
  snapshot) is a replay; it is exact for the strict tier and the measuring stick for the
  tolerant tier (S1.4). The same records are the client's upstream traffic (D12.3) and the
  scripted driver for tests (section 19).
- **P15.2a Everything the tick reads that is not the snapshot is a recorded input.** Three things
  the tick reads were implicit. *Asset versions*: the sim reads bytecode, tables, hitbox skeletons,
  root-motion curves and weapon data, and D8.4a lets their version change under hot reload between
  tick `i` and `i+1` with nothing in the record saying so, so a replay recorded against v3 diverges
  silently under v4 and a save holds a frame whose program counter points into bytecode that no
  longer exists. The snapshot carries the content-set hash, a version change of a sim-consumed asset
  is a recorded input event at the tick it takes effect, a mismatch is reported rather than silently
  diverged (P16.1), and a behaviour in flight (S1.8) observes the change through a relocation hook
  keyed by source UID (a surviving label continues, a missing one unwinds through the declared
  cleanup to the nearest surviving ancestor); presentation-only assets are exempt. *Residency*:
  generation and loading are asynchronous (P8.4), and the tick a cell lands in would otherwise
  depend on disk speed and thread count, shifting every key allocated afterwards (generation inserts
  entities) and failing the strict-tier hash (P19.2) on the first shuffled run. "Cell X became
  resident with generator identity G" and "cell X was dropped" are entries of `inputs[i]`: the
  scheduler decides when a result is admitted, the sim sees only the record, a replay re-admits each
  cell at the recorded tick (waiting for its load if necessary, S1.11), and generated entities are
  allocated keys at the admission tick. *Received state*: on a networked client the tick also
  consumes the server's authoritative diffs (D12.3), so they are recorded next to the input records
  (about 100 KiB/s at 60 Hz for a 64-player arena, the last 30 s ring-buffered), or the most common
  crash site of a multiplayer game has no replay seed (P18.3).

## 16. Persistence, streaming and replays

- **P16.1 A save is a snapshot plus its inputs.** Components, in-flight messages (S1.3), the key
  counter (S2.3), session values in force (S1.12) and the tick id; never the indices (S4.5).
  Schema-generated binary with a version (P7.6) and a text export for diffing and inspection (P7.3).
  The tick function must be able to resume from a save bit-for-bit as it would have from the live
  snapshot.
- **D16.2 Saves are journals of cells.** The P0.5 world cannot be written whole, so the unit of
  persistence is the cell (D3.2b): a save is a base plus an append-only journal of cell *flushes*,
  each the set of pages (entity and grid, D2.8) whose version is newer than the cell's last flush,
  compressed per page (a 1 KiB block page compresses to 100-300 B, a uniform page to 2 B), with
  compaction in the background; autosave is "flush the dirty cells" from a retained snapshot (P2.5),
  off the tick. Per-tick diffs (P2.4) are never journaled: a voxel world would append 300 MB to 1.8
  GB per second, and replays carry inputs, not diffs (P16.1). Retaining the last persisted snapshot
  across the interval costs one copy of every page changed since, bounded by the live state and
  counted in the budget (P17.1). For a generated world the base of a cell is its generator identity
  (D22.1): a page the generator would produce is not stored, on disk or on the wire (D12.4
  baselines), so a pristine cell costs nothing (Minecraft's 10-20 GB worlds are 90% pristine chunks)
  and the cost of a world is proportional to what players and machines changed in it. Land chunks
  are the first grids shaped this way; entities follow.
- **D16.2a Saves are slots with a header, and a world that fits saves whole.** A save document is a
  header plus a payload. The header is a DOM (P7.1) readable without the payload: schema version
  (P7.6), content-set hash and plugin set (P15.2a, D16.5), tick and step ids (S1.15), playtime, and
  project-declared metadata (location, in-game time, thumbnail); slots, autosave rings and quicksave
  rotation are a session-timeline service (S1.12) over slot names, per profile. The payload is the
  base-plus-journal of D16.2 when the world does not fit, and otherwise a whole snapshot serialized
  from a retained immutable snapshot on a background task, so saving never hitches (P2.1); both
  deserialize to the same snapshot. Whole is the common case twice over: a narrative game's 2-5 MiB
  state saved on nearly every choice, where per-cell journals would be 0.6-1 GiB of page-granular
  diffs per playthrough, and a fully live city where every cell is dirty every interval, so a
  journal entry is the full cell, the journal is a sequence of full snapshots and compaction is
  "keep the last" (190 MB per five minutes, 0.6 MB/s). A project declares per component type whether
  it is persisted: a *transient* type (vehicle lane state) is excluded and reconstructed by a game
  rule on load, like D16.3's catch-up, at the price of P16.1's bit-for-bit resume for what it holds.
- **D16.3 Streaming is the same mechanism.** A cell outside every interest set (players,
  pinned loaders) is written out and dropped; reloading reads the same format. Unloaded cells
  do not tick, and catch-up on reload is an explicit game rule (nothing, or an approximation),
  never implicit "simulate the elapsed time". The loaded set is itself derived from state, so
  it is reproducible (S1.4).
- **P16.4 Replays are first-class.** A replay (P15.2) is also a bug report (P18.3), a test
  (P19.1) and the base of a spectator or demo mode; the format is the save format plus input
  records, so there is one serializer.
- **D16.5 Saves outlive plugins.** Over four hundred hours a player installs, updates and removes
  mods, and half the lost-save stories of the modded genres are a type whose plugin is absent on
  load. A save records the plugin set and schema hashes in force (the protocol version of D12.4,
  reused). On load with a plugin missing or its schema changed, component types the engine does not
  know are kept as opaque pages keyed by type UID and schema hash (D4.1a stores them opaquely
  anyway), carried through ticking untouched and written back on the next save, so a mod removed for
  a session and reinstalled finds its data; entities whose only components are unknown stay alive;
  absent systems are not scheduled and their owned components stop changing; messages addressed to
  unknown types are dropped and counted; registry ids (D7.9) map to placeholders. The same policy
  applies to a plugin present but older than the save with no P7.6 converter: opaque, never a
  refusal, which is D12.5's answer for the network and the wrong one for a file. What such a load
  does not claim is the strict-tier hash of a replay (P15.2).

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

- **P20.1 Text is an asset, keyed, addressable per locale.** User-visible strings are never
  literals: they are keys into per-locale string tables that are assets (section 8). A locale is a
  mount layer (P8.2) only as the *default resolution*: every locale's table is addressable by
  itself, so two locales can be resident at once (a dialogue that flips between English and the
  native language on a key press) and switching the language in settings is a hot reload (D8.4a) of
  the string tables, never a restart. Mods localize like the base game: one more layer. Per-locale
  addressability is a property of asset keys, not of string tables: any asset type (audio for
  voice-over, textures with baked text) may have locale variants under one key with a declared
  fallback (another locale, or none: subtitles only). Translation state lives beside each locale's
  table as a machine-owned document (P7.4): per key, the source-text hash the translation matched
  and the reviewer status, so the asset build (P8.3) flags stale translations (5% source churn per
  patch over 70k keys is 3.5k flags per locale) and tracks dependencies per record, not per file, or
  every writer edit recooks every locale.
- **D20.1a Formatted text is a small expression language.** Strings with variable content carry
  inline expressions evaluated at runtime, written by the translator, since word order, plural
  forms and case agreement differ per language: `you have {x} {plural(x, item)} in your pocket`.
  The functions are backed by data, not code: plural categories per locale from CLDR rules
  (Russian: one for 1, 21, 31; few for 2-4, 22-24; many for 0, 5-20, 11-14), noun forms per
  key (singular/plural, cases, gender) from the string table, numbers and dates from locale
  data. The translator chooses the forms, the rules say which applies; no translator writes the
  21-24 rule by hand. Nothing `fmt`-style with positional arguments reaches content.
- **D20.1b Expressions evaluate against a declared context.** D20.1a's `{x}` supplied by "the
  caller" works for UI code and not for seventy thousand writer-authored lines, where the line
  itself says `{player.name}` or `{kim.trust}` and there is no caller. A string's expressions read a
  context of named values: ambient ones the project declares (the player, the world) and the free
  variables the source-locale string names, which the asset build validates every other locale
  against, so the set a line reads is stable across locales. Values come from reflected components
  (D4.1a) of well-known entities (P4.8) read from a snapshot on the presentation timeline (S1.2),
  never from positional arguments. `select(value, forms...)` chooses a form by a runtime value (the
  player's gender, which Russian, French, Polish and Hebrew inflect verbs and adjectives by, in
  lines whose source has no gendered word at all) alongside `plural`. Anything that stores text for
  later (conversation log, journal, save) stores the key and the captured context, never formatted
  text, so it renders in whatever locale is resident (P20.1).
- **P20.2 Text rendering is not Latin-only from day one.** Shaping and bidirectional text are
  in the UI stack from the start (HarfBuzz and FreeType are already dependencies); fonts are
  assets with fallback chains per script.
- **D20.3 Paragraph layout is an engine deliverable with a cache.** Today `FontRenderer` draws from
  a PNG atlas and calls neither FreeType nor HarfBuzz, and the UI has no notion of runs, spans or
  direction. The text stack is the standard pipeline: itemization by script, bidi level (UAX #9) and
  font coverage along a fallback chain (per codepoint, not per script: a CJK face lacks Cyrillic,
  every face lacks some combining marks), shaping per run, line breaking per locale rules (UAX #14,
  CJK and Thai) and placement, producing a layout that keeps cluster-to-span maps for hit testing
  and selection. Shaping costs 0.5-2 us per glyph for Latin and 3-10 us for Arabic, so a dialogue
  screen of three thousand glyphs reshaped per frame is 3-15 ms against a 6.9 ms frame: layouts are
  derived state of S10.2 cached by (key or text hash, locale, style, width, scale) and reused across
  frames, and the per-frame UI work is placement, never reshaping. Rich text is a span tree over the
  string (markup and D20.1a expressions share one grammar), glyphs are rasterized on demand into an
  evictable atlas (a zh/ja run touches 20k+ distinct glyphs), fonts are streamed assets (P8.4) with
  a budget (P17.1), and layout direction (RTL mirroring of containers) is a UI property set per
  locale.

## 21. Worlds and process lifecycle

- **D21.1 A world is an object, not a global.** Play-in-editor (S9.4), a server hosting several
  instances, and tests all put more than one simulation in one process. Services are process-level
  (section 6); a world owns its timeline, snapshots, cells and key counter, and every snapshot and
  message carries its world. Nothing simulation-side is a singleton.
- **P21.2 Startup is services, then worlds.** The engine constructs services (P6.3) from the
  settings document (D7.8), then worlds from documents (S4.2); shutdown is the reverse, and a
  world can be destroyed and recreated without restarting services (editor reload, server map
  change).

## 22. Remaining domains, in one line each

- **D22.1 Procedural generation is strict-tier and staged.** Generation is a pure function of seed,
  position and *generator identity* (the ordered list of stage plugins and their versions, hashed),
  bit-exact across platforms because it is content: two players must see the same world, and two
  installs of one mod set must produce one world, so stage order comes from declared phases and
  UIDs, never from load order. It is staged: stage k of a cell reads stage k-1 of its neighbours
  within a declared halo (structure starts need about 8 chunks, carvers and features spill 1),
  intermediate stages are a regenerable cache owned by the generator service (P6.3), never state,
  and no stage reads the live snapshot, or an edit would change what a later neighbour generates.
  The identity is recorded per cell when it is admitted (P15.2a), a cell is never regenerated under
  a different identity, and seams between identities are the game's problem. Floating point in
  generators is fixed-point or integer, or tested as strict (P19.2); the current `land::Generator`
  samples `float` and is not yet either.
- **D22.2 Navigation is an index, pathfinding a solver.** The navmesh or voxel nav-grid is an
  index (S4.5) over the world, rebuilt incrementally from chunk diffs; pathfinding is a global
  solver (P1.9) answering requests as messages; neural inference for agents is a GPU consumer
  (D10.4) in the tolerant tier.
- **D22.3 Animation splits across timelines.** The sim owns animation *state* (clip, time, blend
  weights, as components, strict-tier where gameplay depends on it: hit frames); the presentation
  evaluates poses from that state per frame and never feeds back. Where the sim itself needs a pose
  (hitboxes, sockets, root motion) it evaluates a reduced gameplay skeleton on demand (D14.6).
- **D22.4 Scripting debugging goes through the control plane.** Breakpoints, watches and
  stepping of entity logic are commands (P9.3) on a paused world (S1.12); native debuggers
  attach to C++ plugins as usual, and `edit()` (S9.4) is how a tool mutates a paused world.
