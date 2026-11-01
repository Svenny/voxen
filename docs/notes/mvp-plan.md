# MVP plan

The implementation plan for the first milestone of the engine rework: what is built, in which
shapes, in which order, and what is left out with the hook that keeps it possible. Item IDs (P2.1,
D7.9) refer to `docs/notes/design-principles-draft.md`, which stays the source of truth; where this
plan narrows a draft item for the MVP it says so, and draft items marked open stay open unless the
MVP cannot be built without an answer. Work item IDs (`cg-3`, `sim-4`) are this plan's own. "Today"
is commit `2d36f465`; "the winbox" is the Windows test machine described in
`docs/notes/bug-reports/README.md`, with Intel and AMD GPUs.

## 1. Purpose and acceptance

The MVP is the set of components the engine cannot be used without, each in its minimal usable
form, plus every interface that would be expensive to retrofit later. Its acceptance target is
today's voxel demo (fly over a generated world, place blocks, a menu) rebuilt on the new
architecture, as a vertical slice through every backbone:

- **Terrain is a plugin** (P4.7). Land declares a block-id grid (D2.8) owned by a well-known
  entity (P4.8). Generation is a pure function run off-tick (D22.1); a chunk enters the world only
  as a recorded admission input (P15.2a).
- **The player is an avatar entity** (S2.3) moved by a fixed-point controller system driven by
  per-step input records (P15.1). Block placement is a command (P15.1a) validated by the avatar's
  system and applied by the grid's single writer (P1.13) one step later (S1.2).
- **The sim is one step function** (S1.15) scheduled as jobs on the task pool (P3.3), publishing
  immutable snapshots (P2.1). The presentation interpolates between the two newest (S1.11) and
  renders land meshes rebuilt from grid diffs (P2.4).
- **Schemas drive data** (draft section 7): settings, components, messages, commands, actions,
  registries and asset meta are declared once and reach C++ through codegen and one DOM. Assets
  resolve through a mount stack (P8.2); shaders and block definitions hot reload (D8.4a), and a
  block-definition change reaches the sim as a recorded input (P15.2a).
- **A headless test** runs the sim from a scenario document (P19.1), checks a strict-tier digest
  across thread counts (P19.2), saves a whole snapshot (D16.2a), reloads it and continues
  bit-for-bit (P16.1).

### The acceptance tests

"Done" is these tests green on the Linux, Windows and headless CI jobs, plus the two manual checks.

- **`scenario:fly_place:t1`, `:t4`** (`plugins/game/tests/scenarios/fly_place.scenario.json5`).
  Joins player 0, flies, places a block at step 92, changes `game.fly.forward_mm_per_s` at step
  120 through the session layer and runs to step 300. Each invocation runs five variants:
  continuous, save/reload at 92 (a `SetBlock` in flight) and at 150, an in-process replay of the
  recorded `StepInputs`, and a shuffled system order. All give equal Full digests at every expect
  point, equal to the golden sidecar; both thread counts give the same Strict digest; the
  `game.fly` change appears in step 120's recorded inputs.
- **`scenario:block_def_reload:t1`, `:t4`.** A scenario asset event makes stone unplaceable at
  step 200; a placement at 210 is rejected; replay and a save/reload at 205 reproduce it; the
  content-set hash differs before and after.
- **`scenario:empty:t1`, `:t4`.** Engine-only smoke scenario over the sim test plugin.
- **`plugins-load`.** An Engine with every in-tree plugin; the headless job loads sim parts only
  and records the same plugin set.
- **Unit suites.** `test-tk`, `test-voxen-core`, `test-voxen-gfx` (CPU cases; `[gpu]` cases skip
  without a device), `test-plugin-land`, `test-plugin-game`, `test-tools`.
- **Manual: parity.** `voxen` on Linux and the winbox (Intel and AMD): fly, place, hotbar, main
  menu (Single player, Exit), pause menu (Resume, Exit); a window screenshot by PID.
- **Manual: hot reload.** Edit `chunk_mesh.slang` and `grass.asset.json5`, run
  `cmake --build <dir> --target voxen-data`: the running `voxen` rebuilds the pipeline within a
  second and recolours the terrain through the material table without remeshing.

"Continues bit-for-bit" is checked with the Full digest on one binary; the cross-thread S1.4 gate
is the Strict digest. Scenarios use the deterministic admission policy throughout, so generation
timing cannot reach the record.

## 2. Architecture after the MVP

### Libraries

- **`extras`, `m`**: INTERFACE, unchanged.
- **`tk`**: STATIC, std-only API, no globals. Gains `dom`, `json`/`toml` on the DOM, `schema`
  (reflection, validation, merge), `hash` (streaming XXH3), `bin` (LE primitives), `vpath`,
  `container`.
- **`voxen-core`**: SHARED, headless. root, os (no window), debug, util, v8g, svc, plugin, types,
  assets, ecs, sim, runtime; legacy `land` until the final cleanup.
- **`voxen-gfx`**: SHARED. window, input, text, ui, gfx, client; the only library linking SDL, VMA,
  Vulkan, FreeType and HarfBuzz.
- **Plugin parts**: SHARED, loaded at startup: `plugins/<id>/<id>.so` (sim part) and
  `plugins/<id>/<id>-gfx.so` (presentation part), `.dll` on Windows.
- **Executables**: `voxen` (launcher), `voxen-headless` (scenarios, save dumps), the test
  executables, `bench-v8g`, `bench-ecs`.
- **Tools**, linking `tk` only: `schemac` (new), `assetc`, `uidgen`.

`VOXEN_BUILD_GFX=OFF` builds core, plugin sim parts, `voxen-headless` and the core tests with no
SDL, Vulkan, slangc or Git LFS objects (P9.2). A separate sim DSO is not made: nothing links sim
without core, and `ecs < sim < runtime` sit at the top of core, so a later cut is a fragment move.

### Module order

One module = one directory = one library. A module may include only modules to its left; the
CMake fragment order is the table, emitted as `layering.json` and checked by
`tools/scripts/check_layering.py` in CI.

```
tk          extras, m < tk
voxen-core  root ≈ os < debug < util < v8g < svc < plugin < types < assets < ecs < sim < runtime
voxen-gfx   window < input < text < ui < gfx < client          (all of core to the left)
plugins     sim part: core + own generated code + headers of USES plugins
            presentation part: core + gfx + own sim part + headers of USES plugins
```

`ui` sits below `gfx` (gfx's UI feature consumes `ui::RenderData`; `gfx/ui` today includes only
itself and `util/scratch*`). The window leaves `os` for `window/` (`voxen::window`), so "os has no
window" holds per directory. Plugins never link each other: `USES` gives headers only, and
`-Wl,--no-undefined` enforces it.

### Process shape

```
voxen / voxen-headless main
  CommonCli (--root, --profile, --plugin-dir, --set) -> svc::InstallLayout
  runtime::Engine::create
    UserStorage(layout.user_dir)
    SettingsDocument pass 1 (core + gfx sections; platform file, user file, --set)
    PluginHost(engine.plugins) .addRegistry(TypeRegistry, SystemCatalog, AssetCatalog,
                                            [PresentationRegistry]) .loadAll()  -> commit
    SettingsDocument pass 2 (plugin sections, input.bindings defaults)
    service factories (core, then plugins) -> startAll() on the main thread -> seal()
  voxen:          SessionControl + client::PresentationRuntime on the OS main thread
                  loop: runFrame(); control.applyPending();
                  menu -> Engine::createSession(SessionDesc) -> runtime::Session
                            owns sim::World + sim::StepScheduler (jobs on TaskService)
  voxen-headless: sim::ScenarioRunner(Env from the Engine) creates worlds directly
```

There is no sim thread: each step is a coroutine job at Simulation priority that runs the
session's between-steps hook, seals the inputs, steps, publishes and reschedules itself with
`setNotBefore` (P3.3, S1.11). Threads outside the pool: the OS main thread (SDL, presentation,
GPU recording), one blocking file-I/O thread, and the pipe-memory GC thread (rework deferred).

### Source tree

```
cmake/            VoxenModules (voxen_module, layering.json), VoxenPlugins, VoxenCodegen
                  (voxen_add_schemas), VoxenAssets (voxen_add_asset_pack, voxen_add_shaders
                  PACK), VoxenTesting (voxen_add_test_executable, voxen_add_scenario_test)
include/tk/       dom dom_path json toml hash bin schema schema_ops schema_model schema_gen
                  vpath container
include/voxen/
  os/             + timer_wait.hpp (window moved out)
  debug/          + invariant.hpp, profile.hpp (no-op macros)
  util/           tick_id.hpp gains StepId/TickId; v8g_* moved out
  v8g/            page_allocator item_layout v8g_store v8g_table v8g_grid morton v8g_diff grid_codec
                  v8g_hash_trie (moved, unchanged)
  svc/            tasks (reshaped), io_buffer, async_file_io_service, service_locator, messaging,
                  install_layout, user_storage, settings
  plugin/         plugin_manifest plugin_entry plugin_registrar plugin_host
  types/          type_registry action_table dense_id_table type_table_record
  assets/         asset_handle asset_ref asset_loader asset_mount asset_catalog asset_service
                  content_set
  ecs/            entity_key ecs_slot ecs_cell frame_pos entity_header component_table
                  entity_snapshot entity_store structural transform_source ecs_index
                  well_known_index prefab entity_io
  sim/            step_types world snapshot snapshot_feed system system_catalog system_context
                  grid_set step_scheduler input_record input_inbox external_input message_store
                  state_hash world_save scenario determinism
  runtime/        engine engine_start_args common_cli session session_control asset_input_source
  window/ input/ text/ ui/ gfx/ gfx/vk/ client/        (voxen-gfx)
src/voxen/<mod>/  sources, *_priv.hpp, *.schema.json5 next to their module
src/app/          voxen_main.cpp, headless_main.cpp
src/shaders/      engine shaders (ui, debug, util, shared) -> core pack
src/sandbox/      bench_v8g.cpp (reworked), bench_ecs.cpp
data/core/        core pack sources: pack.json5, fonts/
plugins/<id>/     plugin.json5, CMakeLists.txt, schema/, include/<id>/, src/sim/, src/gfx/,
                  shaders/, assets/, tests/ (+ tests/scenarios/)
tests/            tk/, voxen/ (-> test-voxen-core), voxen-gfx/, tools/, plugins/ (loader and
                  sim test plugins), data/ (schemas, fixture pack sources), scenarios/
tools/            schemac/, assetc/, uidgen/, scripts/check_layering.py
```

### Directory table (build tree = install layout)

`<root>` is `<build>/<config>/` in the build tree and the prefix of a future install. The root is
derived from the location of the `voxen-core` DSO (`os::Process::getModulePath`), never argv0;
`--root` overrides it.

- `bin/`: executables, `voxen-core`, `voxen-gfx`, tools (build).
- `data/packs/core/`: the cooked core pack, `pack.json5`, `pack.index`, `*.vxa` and raw files;
  vpaths `/core/...`, mount layer 0 (assets).
- `data/settings/platform-<os>.toml`: optional platform layer, read as a plain file by the
  settings loader, never through mounts (settings).
- `plugins/<id>/`: `plugin.json5`, `<id>.so|dll`, `<id>-gfx.so|dll` (build, `PluginHost`).
- `plugins/<id>/pack/`: the cooked plugin pack; vpaths `/<id>/...`, layer 1 + load index (assets).
- `user/<profile>/`: `settings.toml` (human-owned user layer), `pipeline.cache`, `saves/`
  (`svc::UserStorage`).
- `tests/plugins/<id>/`, `tests/packs/<name>/`: test plugins and cooked fixture packs, build tree
  only.

Nothing under `data/` is written at run time. The user directory is outside every mounted tree.
The default profile is `default`.

## 3. Shared vocabulary and contracts

Each name has one owner (in parentheses); other pieces use it as written.

- `tk::UID` and `tk::deriveUid`: authored identity of types, assets, plugins, services and prefabs
  (S2.3). `tk::dom::Value`/`Path`/`Diagnostic`: the one DOM (P7.1), every node with a source
  location. `tk::hash::Digest128`/`Hasher128`/`contentSetHash`: XXH3 digests. `tk::bin`: LE
  primitives. `tk::schema::TypeInfo`: the one per-type descriptor (D4.1a). (codegen)
- `tk::container`: the one chunked binary container, for cooked assets and saves; chunks are
  addressed by a 4-char tag plus an optional 16-byte id. `tk::vpath::Path`: the one virtual path
  grammar (`/core/shaders/ui/ui_basic.vert.spv`, `/land/blocks/stone`). (assets)
- `types::TypeId`/`ArchetypeId`: dense per `TypeRegistry`, one registry per Engine, assigned in
  type-UID order at freeze. `types::ActionTable`: the one action index space.
  `types::DenseIdTable<Id>`: the D7.9 registry type. (codegen)
- `StepId` (+1 every step, snapshot metadata) and `TickId` (+1 on time-advancing steps), in
  `util/tick_id.hpp` (S1.15). v8g versions pages by an internal epoch, never by `StepId`. (sim)
- `ecs::EntityKey` (monotonic, never reused), `Slot` (derived placement, never saved), `CellId`,
  `WORLD_ENTITY` (key 1), `FramePos {int3 frame; float3 offset}` (D4.6), `Transform` with a
  discontinuity `epoch` (D10.1b). (ecs)
- `sim::World`; `sim::Snapshot` and `SnapshotRef`, the only retention handle (P2.5);
  `sim::StepInputs`, the recorded per-step buffer (replay format, later wire format, P15.2);
  `sim::InputInbox`, the one inbox; `sim::IExternalInputSource` (P15.2a). (sim)
- `plugin::PluginHost`/`Registrar`/`IRegistry`, the one plugin contract; `svc::InstallLayout`, the
  only root type. (build)
- `svc::UserStorage`; `assets::Asset<T>` (stable handle), `AssetRef<T>` (immutable version),
  `ContentSet` (the sim-consumed versions a snapshot reads). (assets)
- `svc::SettingsDocument`/`SettingsSnapshot`, the one settings implementation (D7.8). (codegen)
- `svc::TaskPriority {Presentation, Simulation, Background}` (P3.3a), `svc::IoBuffer`,
  `runtime::Engine`, `Session`, `SessionControl`. (runtime)

Contracts that every piece relies on:

- **C-1 Types by UID.** Every persisted or recorded format (save sections, `StepInputs`, bus
  records) names types by UID or carries a TypeId-to-UID table written at record time. In the MVP
  a reader refuses a table that differs from the running registry (D16.5's remap slots in there).
  Typed lookups go through the exported `typeInfo(std::type_identity<T>)` accessor or the UID,
  never a template or function-local static: with hidden visibility and per-DLL instantiation a
  static gives a different id per DSO.
- **C-2 Canonical bytes.** Every fixed-layout schema type has explicit zero-initialized padding,
  asserted `sizeof`/`offsetof` and `is_trivially_copyable`. Raw bytes are then canonical for
  equality, change detection (v8g: "changed" means bytes differ), hashing and saving. Floats hash
  bitwise.
- **C-3 Boundary records.** Every declaration record crossing the plugin boundary
  (`PartDescriptor`, `SystemDesc`, `PhaseDesc`, `TypeInfo`, `ModuleInfo`, the ext records,
  `TransformSourceDesc`) is a standard-layout, trivially copyable struct of scalars, UIDs,
  pointers, function pointers and `{pointer, size}` pairs, with a `static_assert` next to it. The
  C++ registrars are thin layers over them, so a C header or the C# binding (S5.2) can bind the
  same tables (P5.1). This becomes a `conventions.md` rule.
- **C-4 Document headers.** Every document, human-owned included, carries `$type` (type name or
  UID) and `$version` (P7.6). A missing `$version` is an error unless the loader declares an
  explicit `assume_version` (the settings loader does, per section); a version other than the
  current one is refused until converters exist.
- **C-5 One world, one Store.** A world's versioned state (entities, grids, sim tables) lives in
  one `v8g::Store`, published once per step; `sim::Snapshot` wraps its `SnapshotRef` together with
  every side object. Releasing the last ref on any thread only queues; pages are freed by the step
  job.
- **C-6 One container, one serializer per layer.** Sim composes the save; ecs writes and hashes
  entities; v8g writes and hashes grid payloads; assets frames chunks and encodes the content-set
  keys; codegen supplies per-type `encode`/`hash`. Slots are never saved or hashed (S2.3, P16.1).
- **C-7 The sim sees only the record.** No system calls a service or reads settings (S1.12,
  P15.2a, D21.1); asset versions, admissions and session values arrive as `StepInputs` entries.

## 4. Backbones

Each backbone lists its MVP scope, decisions (rejected alternatives in one line), the key public
types, the rework of existing code and its work items. Deferred items, with reasons and hooks, are
in section 6.

### 4.1 Build tree, toolkit and codegen

**Scope.**

- **Tree and CI.** Per-module CMake fragments replace the flat lists; the `voxen-core`/`voxen-gfx`
  split; `VOXEN_BUILD_GFX`; a `linux-headless` preset and CI job (no window-system packages,
  `lfs: false` checkout, a step asserting no file under `data/` or `plugins/` is an LFS pointer);
  the layering check in `check-format`; `VOXEN_SHIPPING` through a generated
  `voxen/build_config.hpp` (P11.3).
- **DOM** (P7.1): `tk::dom::Value` (null, bool, int, uint, float, string, array, ordered object),
  each node with a `SourceRef`; `tk::json` (JSON, JSON5) and `tk::toml` read and write onto it.
- **Merge** (P7.2): one `tk::schema::mergeInto` with policies `replace` and `merge` and the
  `$delete` directive.
- **Codegen** (P7.7, D4.1a): `tools/schemac` (links `tk` only) generates structs, enums, DOM
  conversion, defaults, explicit fixed layouts with asserts, `TypeInfo` tables, binary
  encode/decode/hash, settings descriptors, a module descriptor and the schema hash.
- **Types**: `types::TypeRegistry` collects the generated modules of core and plugins, freezes,
  assigns ids, builds the `ActionTable`; `types::DenseIdTable` is the D7.9 registry type.
- **Settings** (D7.8): `svc::SettingsDocument`, layers defaults, platform, user, session, cli.
- **Hashing**: vendored xxHash (XXH3-64/128), `deriveUid`, `hashString` (replaces `std::hash` UI
  ids), `contentSetHash` (the canonical P15.2a definition).

Schema kinds: `struct`, `enum`, `id`, `constant` (P1.9a caps only); `component`, `message`,
`command`, `admission`; `archetype`, `well_known`, `logical_cell`, `grid`; `document`, `settings`,
`asset`; `registry`, `actions`.

**Decisions.**

1. **Schemas are JSON5 through the DOM**, one `types` object per file keyed by name, key order =
   layout order. *Rejected:* a custom IDL (a third parser); C++ attributes plus a scanner (blocks
   content-only mods).
2. **`TypeInfo` is a C-layout table in `tk`** (C-3): UIDs, integers, `StrRef`/`ListRef`, function
   pointers. *Rejected:* `std::span`/`std::string_view` members.
3. **Array-wise slots, null = trivial** (memcpy, memcmp, raw bytes): one indirect call per page,
   since LTO does not cross the plugin boundary.
4. **Explicit asserted padding in every fixed layout** (C-2). *Rejected:* requiring
   `has_unique_object_representations` (false for every float field).
5. **Tier per field**, with type and module defaults, required for sim records (S1.4a). A leaf's
   tier is the nearest explicit one on its path: `Transform.position.frame` is strict,
   `Transform.orientation` tolerant. *Rejected:* project overrides of whole core types (section 7).
6. **`TypeId` per registry in UID order** (C-1): deterministic for a plugin set, no world lookup on
   typed access. *Rejected:* per-world type tables (buy nothing while foreign plugin sets are
   refused).
7. **Cross-module references are UIDs resolved at freeze; value-type codecs are inline**, so game
   uses `land::BlockId` with land's headers only, and no constant data points into another DLL
   (dllimport addresses are not constant on clang-cl).
8. **Entity references are found by reflection** over `entity` fields, once per type;
   `ConvertContext` gains `entityFromDom`/`entityToDom` for prefabs. *Rejected:* a generated
   remap slot (redundant with the offsets).
9. **Settings flags per field** (D7.8): restart-required by default, opt-in `live` and
   `sim_input`. A type's `sim_input` fields are packed into a generated `<Name>SessionValues`
   record, so replays carry the values in force (S1.12). Restart-required fields keep their startup
   value in `current()`.
10. **Change notification is a polled `generation()`** (presentation per frame, session per step).
    *Rejected:* callbacks on the setter's thread; a control-plane topic (no consumer).
11. **Unknown schema attributes are errors**, so later attributes (replication, quantization) are
    additive. *Rejected:* parsing and storing them unused.
12. **Generated code goes to `<build>/gen/$<CONFIG>/<target>/`**, written only on change, never in
    a PCH. *Rejected:* a config-independent `gen/` (ill-defined under Ninja Multi-Config).
13. **schemac emits the owner's export macro** (`VOXEN_API`, `VOXEN_GFX_API`, `<ID>_API`), so
    gfx and plugins link core's generated accessors under hidden visibility and `--no-undefined`.
14. **A `UInt` DOM kind above `INT64_MAX` only**, so u64 seeds survive JSON; TOML-facing `uint64`
    fields also accept `"0x..."`.
15. **TOML keeps its parser-internal tree**, made private, and converts at the end (1364 tested
    lines stay).

**Interfaces.**

```cpp
namespace tk::schema {
struct TypeInfo {   // standard layout, immutable, reached through exported accessors
  UID uid; StrRef name, module; TypeKind kind; Tier tier;
  uint32_t version; hash::Digest128 schema_hash;
  uint32_t size /* 0 for tags */, align, traits;   // Tag, FixedLayout, HasStrictFields, ...
  ListRef<FieldInfo> fields;
  void (*construct)(void *dst, size_t n) noexcept;                  // schema defaults
  void (*copy)(void *dst, const void *src, size_t n) noexcept;      // null = memcpy (MVP: always)
  void (*encode)(const void *src, size_t n, bin::Writer &);
  bool (*decode)(bin::Reader &, void *dst, size_t n);
  void (*hash)(const void *src, size_t n, hash::Hasher128 &) noexcept;
  void (*hash_strict)(const void *src, size_t n, hash::Hasher128 &) noexcept;   // strict fields
  dom::Value (*to_dom)(const void *src, const ConvertContext *);
  bool (*from_dom)(const dom::Value &, void *dst, const ConvertContext *, dom::Diagnostics &);
  const void *ext;   // ComponentInfo, MessageInfo, CommandInfo, GridInfo, SettingsInfo, ...
};
struct GridInfo { UID element_uid; uint32_t element_size; uint8_t dims, tile_log2[3];
  uint32_t per_frame; UID owner_well_known; uint8_t key_bits[3]; int32_t key_origin[3];
  uint32_t absent_value; };
struct ActionInfo { UID uid; StrRef name; ActionKind kind; ActionDomain domain;
  uint8_t edge_payload; ListRef<StrRef> default_bindings; };
}

namespace voxen::types {
class VOXEN_API TypeRegistry final : public plugin::IRegistry, public tk::schema::TypeResolver {
public:
  void addModule(plugin::PluginId owner, const tk::schema::ModuleInfo &module);
  void commit(const plugin::PluginHost &) override;   // = freeze(): ids in UID order
  TypeId id(UID) const noexcept;
  const tk::schema::TypeInfo *info(TypeId) const noexcept;
  std::span<const tk::schema::TypeInfo *const> typesOfKind(tk::schema::TypeKind) const noexcept;
  const ActionTable &actions() const noexcept;
  tk::hash::Digest128 schemaSetHash() const noexcept;
};
class VOXEN_API ActionTable {   // sim-domain [0, simCount) in UID order (<= 128), then presentation
public:
  std::optional<uint16_t> find(UID) const noexcept;
  const tk::schema::ActionInfo &info(uint16_t) const noexcept;
  const tk::schema::TypeInfo *payload() const noexcept;   // at most one per plugin set
  tk::hash::Digest128 layoutHash() const noexcept;        // in every StepInputs header
  tk::dom::Value defaultBindings() const;
};
template<typename Id> class DenseIdTable {   // immutable, refcounted; reserved ids, placeholder
public:
  // New UIDs appended in UID order; never reassigns
  std::expected<DenseIdTable, std::error_condition> withAdded(std::span<const UID>) const;
  std::optional<Id> find(UID) const noexcept; Id findOrPlaceholder(UID) const noexcept;
};
}

namespace voxen::svc {
class VOXEN_API SettingsDocument {   // not a service; the Engine builds it before services
public:
  explicit SettingsDocument(Files files);                    // platform files, user file
  void addSections(std::span<const tk::schema::TypeInfo *const> types);   // re-merges
  bool addCliSet(std::string_view assignment, tk::dom::Diagnostics &);    // "path=value"
  void setLayerText(SettingsLayer layer, std::string_view toml);         // tests, scenarios
  std::shared_ptr<const SettingsSnapshot> current() const noexcept;   // section<T>(), generation()
  bool set(const tk::dom::Path &, tk::dom::Value, tk::dom::Diagnostics &);   // session layer
  std::string defaultsAsToml() const;                        // --print-default-settings
};
}
```

Schema excerpt; every piece writes in this grammar:

```json5
// plugins/land/schema/land.schema.json5 (abridged)
{ schema_language: 1, module: "land", namespace: "land", uid: "…", tier: "strict",
  imports: ["voxen.ecs"],
  types: {
    BlockDef:   { kind: "asset", uid: "…", version: 1, section: "block", sim_consumed: true,
                  fields: { display_name: "string", color: "uint8x3",
                            placeable: { type: "bool", default: true } } },
    BlockTypes: { kind: "registry", uid: "…", key: "BlockDef", id: "BlockId", id_type: "uint16",
                  reserved: [{ id: 0, name: "AIR", uid: "…" }],
                  placeholder: { id: 1, name: "MISSING", uid: "…" } },
    Terrain:    { kind: "well_known", uid: "…", archetype: "TerrainArchetype" },
    BlockIds:   { kind: "grid", uid: "…", element: "BlockId", dims: 3, tile: [8, 8, 8],
                  per_frame: 32, owner: "Terrain", key_bits: [21, 11, 18] },
    SetBlock:   { kind: "message", uid: "…", version: 1, delivery: "ordered",
                  fields: { block: "int32x3", id: "ref<registry:BlockTypes>" } },
    MAX_ADMISSIONS_PER_STEP: { kind: "constant", type: "uint32", value: 64 },
  } }
```

Type expressions: `bool`, `int8..int64`, `uint8..uint64`, `float32/64`, `uid`, `entity`;
`<scalar>x<N>` and `quat` (mapped to `m`); `string`; `T[N]`, `array<T>`, `map<string, V>`;
`ref<asset:T>`, `ref<registry:R>`; `module.Name`. In fixed-layout kinds `ref<registry:R>` is the
generated id and `string`/`array`/`map` are refused as post-MVP blob fields.

**Rework.**

- `tk/json.*`, `tk/toml.*` onto the DOM (tests ported, a JSONTestSuite subset added, TOML dates
  rejected with a location); `tk/parse_error.hpp` becomes `dom::Diagnostic`; `tk/hash.hpp` and
  `tk/uid.hpp` extended.
- `svc::Config`, `gfx::GfxOptions`, `gfx_options.json5`, the moved `RuntimeConfig`, and
  `makeCliParser`/`patchConfig` in `engine.cpp`: deleted in the cut (`cg-10`). Today's three gfx
  switches survive as `cli` aliases (`--gfx-debugging`, `--gfx-validation`, `--gfx-gpu-name`),
  which existing Windows test runs use.
- `svc/runtime_config.*`: moved once to `gfx/gfx_runtime_options.*`, with
  `EngineStartArgs::addCliOptions`, removing the `svc -> gfx` edge (`engine.cpp:98/231`,
  `gfx_system.cpp:126`).
- `os/window*`: `git mv` to `window/`; six include sites.
- `cmake/VoxenToolchain.cmake`: `MAKE_C_IDENTIFIER` for `DEFINE_SYMBOL` (`voxen-core` would define
  `VOXEN-CORE_EXPORTS`). `voxen/visibility.hpp`: `VOXEN_API` stays; `VOXEN_GFX_API` in a new
  `voxen/gfx/gfx_visibility.hpp` on the 12 gfx/client headers.
- `debug::UidRegistry`: type names registered automatically at freeze.

**Work items.**

- `build-1` Fragments, `layering.json`, `check_layering.py` with an allow-list for `svc -> gfx` and
  `gfx -> client`. Exit: identical binaries; a planted `svc -> gfx` include fails.
- `build-2` `voxen_add_test_executable` generating per-tag ctests. Exit: `ctest -N` unchanged.
- `build-3` `RuntimeConfig` move, window move, core/gfx split, headless preset and job. Exit: the
  old `game` runs on Linux and the winbox; `svc -> gfx` leaves the allow-list.
- `build-4` `getModulePath`/`getExecutablePath`, `InstallLayout`, empty `plugin`/`types`/`ecs`/
  `sim`/`runtime` fragments, `build_config.hpp`. Exit: tests find their root without argv0.
- `cg-1` Streaming hash. Exit: XXH3 known answers; a golden `contentSetHash` on both OSes.
- `cg-2`..`cg-4` DOM, JSON/JSON5 with a writer, TOML. Exit: ported tests; `read(write(v)) == v`.
- `cg-5` Reflection, validate, merge, headers. Exit: every policy at every depth; file:line
  diagnostics.
- `cg-6` Schema model and layout. Exit: a component without a tier, `string` in a component and an
  unknown attribute are rejected with a location; `Transform.position.frame` lowers strict.
- `cg-7` + `build-5` schemac and `voxen_add_schemas`. Exit: a test schema with an import compiles
  into `test-tk`; touching it rebuilds only dependents; `windows-ninja` builds Debug and Release
  from one tree; voxen-gfx links a core-generated `fromDom` on Windows.
- `cg-8` Binary codecs. Exit: round trips; `hash_strict` skips tolerant fields; a golden encoded
  command digest on both OSes; truncated input fails cleanly under ASan.
- `cg-9` `voxen::types` (needs `build-6`). Exit: shuffled registration gives identical ids; a second
  payload type is an error; `DenseIdTable` order and no reassignment; a test plugin and core
  resolve one `TypeId` on Linux and the winbox.
- `cg-10` Settings and the deletion of the old ones (in the cut). Exit: layer order, restart
  freezing, `generation()`, `forEachSessionValues`, the missing-header error; `--set
  gfx.use_validation=true` and `--gfx-validation` both work.
- `cg-11` Slice kinds end to end: game uses `land::BlockId` with no link to land; a scenario's
  `PlaceBlock` with block UIDs records `BlockId`s.

### 4.2 V8G storage

**Scope.** One `v8g::Store` per world (C-5) holding paged copy-on-write tables over a 64-bit key
(S2.2, D4.4) and 3D grids (D2.8); immutable snapshots (P2.1, P2.7); O(change) diffs (P2.4);
deferred reclaim (P2.5); compare-and-publish (D2.9); a canonical grid payload codec; the P2.6
benchmark update. Namespace `voxen::v8g`, the `V8g` prefix dropped.

- **Page allocator** (D17.2, P17.1): one per world with budget tags (live, peak, allocations,
  frees), over 64-byte-aligned `operator new`.
- **Tables**: type-erased `ItemLayout`; 64-item leaves under a COW 64-ary radix directory over the
  leaf key (`item_key >> 6`) whose height grows with the key range, with one root prefix per table;
  one leaf format, a presence mask plus compacted entries (S4.4a); stride 0 = tag.
- **Grids**: 8^3 tiles of 1, 2 or 4-byte elements; tile keys from `MortonLayout3`; one 32^3 chunk
  (4^3 tiles) is exactly one bottom directory node; tiles *uniform* (inline in the slot, no
  allocation) or *dense*; palette's kind byte reserved.

**Decisions.**

1. **64-item leaves**: S4.4a's mask needs 64, and smaller pages cut copy and retention together
   (about 32 MiB retained instead of 68.7 at 10k random updates). *Rejected:* 1024-item pages.
2. **One directory for tables and grids, growing height plus a root prefix**: slot windows and
   biased Morton codes share long top prefixes; 1M slots take 3 levels. *Rejected:* a fixed depth
   (single-child chains); the hash trie (hash order); a flat vector (O(pages) publish).
3. **The COW version is an internal epoch, never saved**: `StepId` repeats after a load or rollback
   and would break the predecessor shortcut and stale-write checks.
4. **Per-item change = a per-leaf `written` mask plus `pred_epoch`**: adjacent diffs compare only
   written items. *Rejected:* per-item versions (12.5% to 200% overhead).
5. **No compare-on-seal**: only `publishLeafIfChanged`/`publishTileIfChanged` compare; the dev
   checker counts published leaves equal to their predecessor as a D2.9 finding (P18.4).
   *Rejected:* reverting no-op copies at every seal.
6. **"Changed" means bytes differ** (needs C-2).
7. **Atomic refcount only on the store snapshot; frees deferred to the step job.** *Rejected:*
   `shared_ptr` per page (977 atomics per publish in the bench); epoch reclamation (cannot free a
   dropped rollback branch).
8. **Tiles are retained through snapshots**: `TileRef` is a view under a held ref; off-tick
   producers hand a move-only `DetachedTile`. *Rejected:* an atomic shared-tile type.
9. **Absent is not uniform**: a missing tile is not resident and samples as `absent_value`.
10. **No save framing and no slot-keyed table saves in v8g** (C-6); its only payload is the grid
    codec, because tile keys are identity. No retention policy either: sim owns the ring.
11. **The hash trie moves unchanged; its reshape waits for legacy land's deletion.** The flat map is
    deleted (no users).

**Interfaces.**

```cpp
namespace voxen::v8g {
// Thread-safe; one per world. Over budget logs one warning per tag and never fails.
class VOXEN_API PageAllocator {
public:
  BudgetTag registerTag(std::string_view name, uint64_t budget_bytes = 0);
  void *allocate(size_t bytes, BudgetTag);   // 64-byte aligned
  void deallocate(void *, size_t bytes, BudgetTag) noexcept;
};
// A tag (size 0) maps to stride 0; blob types are refused in the MVP
VOXEN_API std::optional<ItemLayout> itemLayoutOf(const tk::schema::TypeInfo &) noexcept;
VOXEN_API std::optional<GridLayout> gridLayoutOf(const tk::schema::TypeInfo &grid) noexcept;

class VOXEN_API Store {   // NOT thread-safe: beginStep/publish/reclaim serialized by the step job
public:
  TableId addTable(const ItemLayout &, BudgetTag, std::string_view name);   // setup only
  GridId addGrid(const GridLayout &, BudgetTag, std::string_view name);
  StepToken beginStep(StepId step);
  TableWriter writeTable(const StepToken &, TableId);   // one per table per step
  GridWriter writeGrid(const StepToken &, GridId);
  TableView headTable(TableId) const noexcept;          // only with no live writer (D1.14)
  SnapshotRef publish(StepToken &&);                    // O(tables) plus seal work
  size_t reclaim() noexcept;                            // frees queued snapshots
  void resetTo(const StoreSnapshot &);                  // rollback, later
};
class VOXEN_API TableWriter { public:
  void *write(uint64_t key); void *insert(uint64_t key); void erase(uint64_t key);
  void move(uint64_t from, uint64_t to);                // repack
  bool publishLeafIfChanged(LeafKey, uint64_t presence, const void *entries);
  template<typename F> void forEachWrittenLeaf(F &&) const; };
class VOXEN_API GridWriter { public:
  std::span<std::byte> mutableTile(uint64_t tile_key);  // private dense copy
  void adoptTile(uint64_t tile_key, DetachedTile &&);
  bool publishTileIfChanged(uint64_t tile_key, std::span<const std::byte> dense512);
  void dropRange(KeyRange); };
class MortonLayout3 { public: constexpr MortonLayout3(m::uint3 axis_bits, m::int3 origin);
  uint64_t encode(m::int3) const noexcept; m::int3 decode(uint64_t) const noexcept; };
namespace Diff {   // (older, newer), fn(key, older, newer), null = absent
template<typename F> void visitItems(const TableView &, const TableView &, KeyRange, F &&);
template<typename F> void visitTiles(const GridView &, const GridView &, KeyRange, F &&);
}
namespace GridCodec {   // canonical LE tile records in key order; the hash covers those bytes
VOXEN_API void write(const GridView &, KeyRange, tk::bin::Writer &);
VOXEN_API void hash(const GridView &, KeyRange, tk::hash::Hasher128 &);
VOXEN_API std::expected<void, std::error_condition> read(GridWriter &, std::span<const std::byte>);
}
}
```

`GridView` adds `sample`, `expandBox` (the 34^3 bordered copy, YXZ) and a caching sampler; typed
wrappers `TTableView`/`TTableWriter<T>` and `TGridView`/`TGridWriter<T>` cover any trivially
copyable 1/2/4-byte element. `PageAllocator` and `DetachedTile::encode` are thread-safe; node and
leaf refcounts are touched only by the serialized Store calls and the subtree's writer.

**Rework.** `util/v8g_hash_trie*.hpp` and its test: `git mv` to `v8g/`, namespace `voxen::v8g`,
with the concepts and `V8gMapItem` it uses folded in, in the same commit as the `land_state.*`
instantiations and `bench_v8g.cpp`. `util/v8g_flat_map*`, `v8g_concepts.hpp`, `v8g_helpers.hpp`:
deleted. `src/sandbox/bench_v8g.cpp`: reworked on `v8g::Store`, linking `voxen-core`, with the
legacy 1024-item store as the baseline.

**Work items.**

- `v8g-0` Module move. Exit: both OSes build; `bench-v8g` reproduces the trie row of P2.6.
- `v8g-1` `PageAllocator`. Exit: an 8-thread stress is TSan-clean; stats balance to zero.
- `v8g-2` Tables, Store, snapshots, seal, publish, reclaim. Exit: a model test against `std::map`
  per retained snapshot over 10^4 random steps at all densities; a ref released on another thread is
  freed only by `reclaim`; debug death on a stale token and a second writer.
- `v8g-3` Diffs and active sets. Exit: matches the model; work bounded by changed leaves times
  depth.
- `v8g-4` Grids. Exit: a model test; Morton round trips with negative coordinates; a chunk's 64
  tiles in one node; an all-equal dense write becomes uniform; `DetachedTile::encode` from 8
  threads.
- `v8g-5` Grid codec. Exit: write, read into a fresh Store, write is byte-identical and
  hash-stable; truncation at every 8-byte offset errors cleanly.
- `v8g-6` Benchmark, the gate for decisions 1 and 2: (a) random updates at retention 1/4/64 against
  the legacy store; (b) key-to-slot map plus header under 1%/step churn; (c) a density sweep; (d) a
  Morton grid with a moving stencil front. Thresholds: (a) 100k/R1 within 1.25x of the legacy tick
  and under 40 MiB retained at 10k, else revisit the leaf size; (d) over 20% of sampling in descent
  moves path compression into the MVP. Results go into the draft's P2.6 section.
- `v8g-7` Stats and debug invariants exposed to sim's checks.

### 4.3 Assets

**Scope.** `tk::vpath` and `tk::container`; a static mount stack of cooked packs (P8.2); a
registry with a dual index (P8.1); cook-time meta layering (P7.2); stable handles over refcounted
immutable versions (D8.4a); coroutine loaders (P8.4); hot reload by `pack.index`; the content set
of sim-consumed assets (P15.2a); `svc::UserStorage`; `assetc` as the cooker, CMake as the DAG
executor (P8.3).

- **Packs**: `core` (UI and debug shaders, fonts; only with gfx), `land` (block definitions; land
  shaders with gfx), fixture packs (documents only, no SPIR-V, no LFS).
- **Document assets**: `<stem>.asset.json5` metas merged at cook time (schema defaults, the
  `_dir.meta.json5` chain outer to inner, the asset meta) with codegen's `mergeInto`.
- **Raw assets**: files whose bytes are the asset (SPIR-V, fonts); the pack manifest maps the
  extension to a type UID; UID = `tk::deriveUid(type_uid, vpath)`.
- **No asset types in core**: presentation registers `shader.spirv` and `font.sfnt`, land
  `land.BlockDef`. A headless process indexes shader and font entries but never loads them.

**Decisions.**

1. **Merge at cook time**: the build is the place for derivation (P8.3), archives must be
   self-contained, and the runtime needs no schema model to resolve an asset.
2. **Mounts are cooked packs only**; the reload loop is edit, `cmake --build --target voxen-data`,
   reload. *Rejected:* development source mounts (two code paths per type; P8.2, P9.1).
3. **Change detection is a stat of each loose pack's `pack.index`** at most every
   `assets.poll_interval_ms`; `assetc index` writes it last and atomically, a content-hash diff
   bumps exactly the changed assets, and the load re-checks the hash of what it read. *Rejected:*
   watching every cooked file; a native watcher with two OS backends.
4. **Override by UID; the path is the human index** (P8.1, D5.6). Two mounts on one layer
   providing one UID is a startup error naming both.
5. **Strict lowercase ASCII vpaths fixed at cook time** from `mount_point`: one spelling
   (`/core/x`, never `core:/x`), byte equality everywhere.
6. **Shaders and fonts are raw assets with vpath-derived UIDs.** One `.spv` per entry point from
   today's slangc rule (now `voxen_add_shaders(... PACK ...)`), the font file as-is. P8.1 holds:
   every asset has a UID and a path, no data refers to a shader or font by UID, and one vpath
   derives one UID. *Rejected:* `assetc` running slangc into per-module containers with authored
   meta UIDs (duplicates a working rule); an SDF ASCII atlas (the design D20.3 replaces).
7. **One asset per block type, asset UID = block type UID** (D7.9).
8. **Loaders are coroutines over a `LoadContext`**, so progressive loads (P8.4) change no loader.
9. **Handles are slot indices, contents intrusively refcounted versions**; consumers poll a stamp
   or drain a change feed, no callbacks. A snapshot's `ContentSet` holding refs gives retention for
   free (P2.5).
10. **The content-set hash covers `(uid, content_hash)` of sim-consumed assets in UID order**;
    content hashes exclude cooker versions. Saves store the key list, so a mismatch names the asset.
11. **`UserStorage` is synchronous, not a service**: the user settings layer needs it before
    services exist.
12. **`runtime::AssetInputSource` lives in `runtime`**: it implements a sim interface and calls a
    service, and only `runtime` sits above both.

**Interfaces.**

```cpp
// Header magic[4] ("VXA1" | "VXSV"), then a table of { tag[4], flags, id[16], offset, size }
namespace tk::container {
class Writer { public: explicit Writer(Tag magic);
  void addChunk(Tag tag, tk::UID id, std::vector<std::byte> data);   // (tag, id) unique
  std::vector<std::byte> finish() &&; };                            // deterministic bytes
class View { public:
  static std::expected<View, std::string> open(std::span<const std::byte>, Tag magic);
  std::span<const std::byte> chunk(Tag tag, tk::UID id = {}) const noexcept; };
}
namespace voxen::svc {
// Rooted at InstallLayout::user_dir; names are vpath-checked; an empty root disables it
class VOXEN_API UserStorage {
public:
  std::expected<std::vector<std::byte>, std::error_condition> readFile(std::string_view) const;
  std::expected<void, std::error_condition> writeFileAtomic(std::string_view,
    std::span<const std::byte>) const;
  std::expected<std::filesystem::path, std::error_condition> resolve(std::string_view) const;
};
}
namespace voxen::assets {
template<CAssetType T> class Asset { public: uint32_t stamp(const AssetService &) const noexcept; };
template<typename T = void> class AssetRef;   // refcounted, immutable AssetVersion<T>
class IAssetLoader { public: virtual UID assetTypeUid() const noexcept = 0;
  virtual svc::CoroSubTask<std::expected<AssetRef<>, std::error_condition>>
  load(LoadContext &) = 0; };
template<CAssetType T> class DocumentAssetLoader;   // META -> generated fromDom<T>()
template<CAssetType T> class BytesAssetLoader;      // T(svc::IoBuffer &&)
class VOXEN_API AssetCatalog final : public plugin::IRegistry { public:
  void addLoader(plugin::PluginId owner, std::unique_ptr<IAssetLoader>);   // one per type
  void commit(const plugin::PluginHost &) override; };   // packs: core 0, plugins 1 + index
class VOXEN_API AssetService final : public svc::IService {
public:
  template<CAssetType T> Asset<T> find(UID) const;
  template<CAssetType T> Asset<T> findByPath(const tk::vpath::Path &) const;
  template<CAssetType T> AssetRef<T> acquire(Asset<T>) const;            // never blocks
  uint64_t whenResident(std::span<const AssetSlot>, svc::TaskPriority);  // one counter
  void poll();                                              // thread-safe, rate-limited
  void drainSimChanges(uint64_t &cursor, std::vector<AssetRef<>> &out);
  svc::CoroSubTask<std::expected<InitialContent, std::error_condition>>
  makeContentSet(svc::TaskPriority);
  svc::CoroSubTask<Resolved> resolveContentSet(std::span<const ContentSetKey>, svc::TaskPriority);
};
class ContentSet {   // immutable, sorted by uid
public:
  template<CAssetType T> const T *get(UID) const noexcept;
  tk::hash::Digest128 hash() const noexcept;                                    // contentSetHash
  std::shared_ptr<const ContentSet> with(std::span<const AssetRef<>> changes) const;   // COW
};
struct AssetVersionEntry { UID uid; UID type_uid; uint64_t content_hash[2]; };   // recorded part
template<CAssetType T> AssetRef<T> makeDetachedVersion(UID, T contents, tk::hash::Digest128);
}
```

Formats (all with `$type` and `$version`, C-4):

- **Pack manifest** `pack.json5`: `{ $type: "voxen.assets.Pack", $version: 1, uid, name: "land",
  mount_point: "/land", raw: { ".spv": "<shader.spirv uid>" } }`.
- **Asset meta** `plugins/land/assets/blocks/stone.asset.json5`: `{ $type: "land.BlockDef",
  $version: 1, uid: "…", block: { display_name: "Stone", color: [120, 120, 120] } }`; data under
  the type's `section` key; vpath `/land/blocks/stone`. **Directory meta** `_dir.meta.json5`:
  `{ $type: "voxen.assets.DirMeta", $version: 1, block: { solid: true, opaque: true } }`.
- **Cooked asset** `<pack>/<dir>/<stem>.vxa`: chunks `AHDR` (UIDs, schema version, content hash,
  reserved input hash, vpath, `SIM` flag) and `META` (merged data as canonical compact JSON).
- **Index** `<pack>/pack.index`: machine-owned JSON, one record per line sorted by path,
  `{ uid, path, type, file, size, hash, sim, raw? }`; an LFS pointer file is refused.
- **Content hash**: `hash128` of `META` for documents, of the file for raw assets.

CMake: `voxen_add_asset_pack(<target> NAME n MANIFEST m OUTPUT_DIR d [SOURCES dir [PREFIX p]]...
[RAW src dest]...)` adds one `assetc cook` per meta with a depfile and makes the index depend on
every output; `voxen_add_shaders(<target> PACK <pack> SOURCE_DIR ... SHADERS ...)` appends `.spv`
outputs. slangc discovery and the LFS check run on the first `voxen_add_shaders` call, which only
gfx `CMakeLists.txt` make. `voxen-data` (ALL) aggregates packs; executables depend on it.

Hot-reload flow for a sim-consumed asset: `pack.index` rewritten, `poll()` re-reads it and loads
the new version on a Background task; the session hook posts `AssetVersion{uid, hash}` with the
`AssetRef<>` as payload; the consuming step switches `snapshot.contentSet()` (section 4.5).
Scenario asset events bypass the service: the runner builds a detached version (contents to DOM,
merged with the override, `fromDom`) and posts the same entry.

**Rework.** `assets/asset_base.hpp`, `asset_decoder.*`: deleted; `asset_service.*`: replaced
(`SERVICE_UID` kept). `svc::FileManager`: deleted; its users are `vk_pipeline.cpp:85` (shader
assets), `font_renderer.cpp:276` (deleted), `vk_pipeline_cache.cpp:35,82` (`UserStorage`) and
`config.cpp` (deleted). `os::File`: `FileFlagsBit::AsyncIo` removed. `tools/assetc`: `cook`,
`index`, `dump`; `font_atlas.*` and the FreeType link go; split into `assetc-lib` + `assetc`.
`cmake/VoxenAssets.cmake` replaced, `VoxenShaders.cmake` folded in, `add_dependencies(voxen
voxen-shaders)` becomes `voxen-data`. `data/core-assets/` becomes `data/core/` (fonts with
lowercase destination names; the font meta and `core-assets.json5` go).

**Work items.**

- `as-1` `tk::vpath`, `tk::container`. Exit: bad paths rejected (`..`, uppercase, device names);
  container round trip with ids; truncated tables rejected; deterministic bytes.
- `as-2` `UserStorage` (needs `build-4`). Exit: escaping names rejected; atomic writes leave no temp
  file.
- `as-3` `assetc`, `voxen_add_asset_pack`, `voxen_add_shaders(PACK)`, packs (needs `cg-5`). Exit:
  merge order asset > inner dir > outer dir > default; `$delete`; a missing `$version`, duplicate
  UID and bad vpath fail with file:line; a headless configure never runs slangc.
- `as-4` `AssetService` (needs `rt-4`, `build-6`). Exit: lookup by UID and path; override across
  layers; same-layer conflict; two loaders for one type rejected; a corrupted file never publishes.
- `as-5` Presentation wiring, in the series of `pr-3`: pipelines and the text face resolve through
  `findByPath`, startup waits on `whenResident`. Exit: `voxen` renders on Linux and the winbox; no
  `FileManager` remains.
- `as-6` Hot reload. Exit: one changed hash in a fixture index bumps exactly that stamp while an old
  `AssetRef` stays valid.
- `as-7` Content set and sim inputs (with sim, runtime, slice). Exit: the hash is independent of
  mount and load order; a released snapshot frees the old version; loading a save against a changed
  definition reports the UID.

### 4.4 ECS

**Scope.**

- **Keys** (S2.3): 64-bit monotonic, never reused; 0 null, 1 `WORLD_ENTITY`, 2..15 reserved for
  engine-fixed entities, 2^62 and up reserved for prediction, bit 63 marks a prefab-local
  reference. The counter is saved.
- **Slots** (S2.3): `window << 16 | rank`, derived, never saved; order `(CellId, archetype rank,
  key)`; a cell owns a fixed 2^16 window, handed out monotonically.
- **Tables** (D4.4) in the world Store: the key-to-slot map, the always-present header table (key,
  cell, archetype, flags, a 128-bit attached-type set), the cell table, one slot-keyed table per
  component type. Tags are stride 0.
- **Archetypes**: required components attached together with defaults, plus a cell rule; rank =
  archetype UID order; one archetype per entity for life.
- **Cells**: logical only (D3.2a): the world cell and the schema's `logical_cell`s, created at world
  creation; explicit `moveToCell` requests.
- **Boundary** (S2.3, D3.2b): `EntityStore::finishStep` at the end of every step in all S1.15
  configurations merges requests, allocates keys and re-slots touched cells.
- **Indices** (S4.5): `IndexSet` held by `sim::Snapshot`, rebuilt after publish; one instance,
  `WellKnownIndex`.
- **Well-known entities** (P4.8): `EntityStore::seed` creates the world entity and the schema's
  `well_known` entities in UID order (keys from 16).
- **Prefabs** (S4.2): single-level documents compiled once to `PrefabTemplate`s; instantiation
  remaps local references and never touches the DOM in-step (D6.4).
- **Positions** (D4.6): `FramePos` as a schema struct; `FramePosMath` free functions with
  contraction off; the frame size a world constant from the Init record.
- **Transform** (P1.13, D5.6): one writer, engine system `sim.TransformWrite`, deriving it from
  mover components through registered `TransformSourceDesc`s.
- **Persistence** (P16.1, P16.4): the `ENTS` chunk and `hashEntities` over the same walk; a
  dev-build invariant checker (P18.4).

**Decisions.**

1. **Slot order is a pure function of state**, so a reloaded world iterates identically (P16.1).
   *Rejected:* swap-remove insertion order (history-dependent); an order-independence contract
   (unenforceable). Cost, measured by `bench-ecs`: shifting rows inside touched cells.
2. **Keys only at the boundary; spawners get the key by message.** A spawn has no handle in its
   step; `notifyOrigin(tag)` reports `sim.Spawned{tag, key}` at i+1. Requests merge by a stable
   sort on `(system ordinal, origin)`, one task per origin per system, so the order is independent
   of the task split. *Rejected:* pending handles patched at the barrier; per-task key blocks.
3. **Component add and remove are in-step writes by the owner**; only spawn, despawn and cell moves
   are structural. The boundary reconciles header type sets from v8g leaf diffs.
4. **Logical cells only**: avatars in `game.Players` (D3.2c), terrain and the world entity in the
   world cell. *Rejected:* spatial cells now (with grids not owned per cell, one would own one
   avatar).
5. **The frame size is a world constant, not a component** a system could write.
6. **No own publish; `EntitySnapshot` is a view over the world's store snapshot** (C-5).
   *Rejected:* an own `shared_ptr` snapshot (two retention handles per world).
7. **One engine `Transform` owner fed by mover sources**: each mover owns its intent component and
   the writer reads it (P1.13). *Rejected:* the fly controller owning `Transform` (blocks every
   other mover; D5.6 replacement becomes "replace the fly controller").
8. **Indices are built after publish**, so they describe the snapshot they are published with and
   may store slots (S4.5).
9. **One state-order walker behind save, hash and dump** (P16.4): columnar per type over a
   state-ordered entity list, which D4.1b and D16.5 need, hash-stable by construction.

**Interfaces.**

```cpp
namespace voxen::ecs {
struct EntityKey { uint64_t value = 0; }; constexpr EntityKey WORLD_ENTITY { 1 };
struct Slot { uint64_t value = UINT64_MAX; }; struct CellId { uint64_t value = 0; };

template<CComponent T> class TableView { public: const T *find(Slot) const noexcept;
  template<typename F> void forEach(F &&) const; };              // fn(Slot, const T &)
template<CComponent T> class TableWriter { public:
  T &write(Slot); T &add(Slot, const T &); void remove(Slot); };
template<CComponent P, CComponent... R> class Join { public:   // slot order
  template<typename F> void forEach(F &&) const; };   // fn(Slot, const P &, const R &...)

class VOXEN_API EntitySnapshot {   // view; valid while the sim::SnapshotRef lives
public:
  std::optional<Slot> resolve(EntityKey) const noexcept;
  template<CComponent T> const T *get(EntityKey) const noexcept;
  template<CComponent T> TableView<T> table() const noexcept;
  const EntitiesConfig &config() const noexcept;               // frame_size_metres
};
class VOXEN_API EntityStore {
public:
  EntityStore(const types::TypeRegistry &, v8g::Store &, v8g::BudgetTag);
  void beginStep(v8g::StepToken &, EntitySnapshot previous);
  template<CComponent T> TableWriter<T> writer(const WriteGrant &);   // from SystemDesc::writes
  template<CComponent T> TableView<T> current() const;                // D1.14 head view
  BoundaryReport finishStep(std::span<StructuralRecorder *const>);
  SeedReport seed(const WorldSeed &);
  std::expected<void, std::error_condition> load(tk::bin::Reader &ents);
};
class VOXEN_API StructuralRecorder {   // one per task
public:
  // SpawnBuilder: .set<T>(value), .cell(logical_cell), .notifyOrigin(tag)
  SpawnBuilder spawn(EntityKey origin, types::ArchetypeId);
  void spawnPrefab(EntityKey origin, const PrefabTemplate &, uint32_t user_tag = 0);
  void despawn(EntityKey origin, EntityKey target);
  void moveToCell(EntityKey origin, EntityKey target, CellId);
};
struct TransformSourceDesc { UID uid; UID source_type;   // boundary record; replaceable by uid
  void (*derive)(const void *source, Transform *io, size_t n) noexcept; };
VOXEN_API void hashEntities(const EntitySnapshot &, HashScope, tk::hash::Hasher128 &);
VOXEN_API void writeEntitiesSection(const EntitySnapshot &, tk::bin::Writer &);
VOXEN_API std::expected<std::shared_ptr<const PrefabTemplate>, tk::dom::Diagnostics>
compilePrefab(const tk::dom::Value &doc, const types::TypeRegistry &);   // `base` is rejected
}
```

`ENTS` body: format version, `EntitiesConfig`, `next_key`, `next_cell_id`; cells (`CellRecord`,
`CellId` order); archetype UIDs; entities `{key, cell index, archetype index}` in state order; one
column per persisted component type in UID order `{type UID, schema hash, element size, presence
bitmap over the entity list, elements via TypeInfo::encode}`. No slots, windows, `TypeId`s or
ordinals. Strict scope hashes the structure plus each column's strict fields; Full hashes every
persisted column, and with trivial slots equals `hash128` of the body. Load refuses an unknown
column or a schema-hash or size mismatch, verifies state order and rebuilds slots, the key map and
headers.

Order inside a step: `Store::beginStep`, `EntityStore::beginStep`; phases (`seal` at each writer
phase's join); `finishStep`; sim turns the report into bus records; `Store::publish`;
`Store::reclaim`; index rebuild over the published view; `sim::Snapshot` assembly.

**Rework.** `world::Player` becomes avatar components (slice); `world::State`'s player half
becomes `Snapshot::entities()`; `GameView`'s camera math becomes `FramePosMath::cameraRelative`.
The test helpers gain `StringMaker`s for the ids and `FramePos`.

**Work items.**

- `ecs-1` Ids and `FramePosMath`. Exit: normalize at negative frames and exact boundaries;
  identical results at `-O0` and `-O3`.
- `ecs-2` Type contract with codegen; hand-written `TypeInfo`s for six test types. Exit: reflected
  entity-ref offsets match hand-computed ones.
- `ecs-3` Store core (needs `v8g-2`). Exit: writes never change a published snapshot; `Join` visits
  exactly the entities with all types; a test DSO and core resolve one component to one table.
- `ecs-4` Recorder and boundary. Exit: 10^4 random steps against a model with invariants each step;
  1, 3 and 17 recorders over a shuffled split give identical snapshots and hashes.
- `ecs-5` Indices, well-known entities, `seed`. Exit: keys in UID order whatever the registration
  order.
- `ecs-6` Transform sources. Exit: two sources over disjoint sets; an entity reached by two is
  caught.
- `ecs-7` `ENTS` and `hashEntities`. Exit: save, load, equal bytes and hash after a window
  renumbering; a churned world round-tripped then stepped 100 steps equals the unbroken run.
- `ecs-8` Prefabs. Exit: internal references remapped; diagnostics name file and field.
- `ecs-9` `bench-ecs`: 10k/100k entities churned per step at 1/4/64 retained snapshots.

### 4.5 Sim

**Scope.**

- **World** (D21.1): `sim::World` owns its `PageAllocator`, `Store`, `EntityStore`, `GridSet`,
  message store, inbox, external input registry and retention ring.
- **Step function** (S1.15): `StepConfig` (when, time advance, phases by traits, inputs,
  retention); zero-time steps deliver no messages and leave in-flight ones untouched.
- **Systems** (S1.5, P1.6, P1.13): a phase DAG by UID; systems declare phase, owned types, reads,
  policy, granularity and trait masks; writer conflicts are startup errors; D5.6 replacement by UID
  in plugin order. MVP: policy `Always`, granularity `Single`; systems of one phase run in
  parallel, phases join in topological order (P3.4).
- **Post-boundary group** (S4.5): after the ecs boundary and the Store publish, index rebuilds and
  invariant checks run under every trait set, with no plugin opt-out.
- **Pacing** (S1.11): fixed rate, catch-up cap, time stretch; dt = base period x recorded scale.
- **Snapshots** (P2.1, P2.5): one immutable refcounted `sim::Snapshot` per step; `SnapshotFeed`
  gives the newest two with nominal times and the tick period; the ring keeps `max(H, retention)`,
  H = 1.
- **Input** (P15.1, P15.1a): one thread-safe `InputInbox`, sealed at step start into an immutable
  `StepInputs`; commands become messages from the player entity to its avatar in their stamped step.
- **External inputs** (P15.2a): sources `collect` between steps; payloads are not recorded; asset
  versions are a fixed entry kind applied in the step prologue.
- **Registries** (D7.9): one `DenseIdTable` per schema `registry`, on the snapshot, allocated in
  the prologue, saved, hashed in Strict scope.
- **Bus** (S1.2, S1.3, D6.4): per-system outboxes merged at the barrier into a sorted
  `MessageStore` in the snapshot; ordered unicast at i+1; dead targets dropped and counted.
- **Session values** (S1.12): `SessionValues` entries written by the engine system
  `sim.SessionApply` to components on the world entity (P4.8), so saves carry them.
- **Digest, save, scenarios**: Strict and Full digests with per-section sub-digests; whole-snapshot
  save and load; one text dump; the scenario document and `ScenarioRunner` (P19.1, P19.2).

**Decisions.**

1. **Two counters**; the Init step is step 0, tick 0. *Rejected:* one counter plus a flag (breaks
   timers in zero-time steps).
2. **dt = base period x recorded scale** (S1.15 as written, Q16.16).
3. **Phase selection by trait masks** (`require` default `TimeAdvances`, `exclude` default `Init`);
   index maintenance is outside the masks, so no default can skip it.
4. **Partitions will be data-defined**, never `num_threads` (S1.4); MVP: one task per system, and a
   shuffle hook permutes enqueue order.
5. **Message order** `(deliver_tick, target, sender, send_tick, system, task, emission)`.
   *Rejected:* a per-sender atomic counter (arrival order).
6. **Commands are delivered in their stamped step** by the engine system `sim.CommandInjection`,
   targeted at `PlayerControl.avatar` from snapshot i (the player entity if none).
7. **A minimal engine player concept**: `sim.Player`, `sim.PlayerControl{avatar}`, `sim.Possess`,
   and the spawn tag `SpawnTags::POSSESS`. A game system spawns an avatar with the player entity as
   origin and `notifyOrigin(POSSESS)`; `sim.PlayerSystem` (`PHASE_INPUT`) applies the returned
   `Spawned` and any `Possess` to `PlayerControl.avatar`. Avatars stay game logic. *Rejected:* a
   game-side bookkeeping component plus a `Possess` per bind.
8. **World creation is `EntityStore::seed` plus the Init step.** The world document is converted
   outside the step into typed records: `sim.WorldInit` and one record per plugin `init_section`
   struct (no DOM on the sim path, D6.4). The Init step records them, one `AssetVersion` entry per
   initial content entry and the session values, so a replay from an empty world is complete.
   Plugins register Init-trait systems, not callbacks.
9. **Registries are snapshot side objects allocated in the prologue** from the step's `AssetVersion`
   entries: per registry whose key asset type matches, `withAdded` over the missing UIDs in
   ascending order. The Init step's entries make this the creation-time allocation as well, so
   there is one site. Ids are never reassigned. This reads D7.9's "first use" as "the first step
   the definition exists". *Rejected:* a v8g table on a well-known entity with a rebuilt UID index
   (a new table kind and writer system for the same semantics); a blob component; allocation only
   at creation (a hot-reloaded block would have no id).
10. **One input record shape**: per player per step the process-wide sim-domain button bitset,
    at most one project payload struct (orientation lives there), timestamped edges with optional
    payload copies, `input_seq`, and the presented step and alpha (D10.1a hook, unused). The header
    carries `ActionTable::layoutHash()`; a mismatch refuses a recording. *Rejected:* per-action-set
    typed frames.
11. **Scenarios admit deterministically** (`collect` is a pure function of `latest`); live play
    admits when ready; both record the admission step. The in-process replay reuses a recorded
    payload when it is a shareable immutable object (an `AssetRef`) and otherwise asks its source to
    `rematerialize` it: land's tiles are move-only `DetachedTile`s consumed once, so land
    regenerates them from the recorded entry.
12. **Two digest scopes**: Strict is the cross-thread gate, Full checks save/reload and replay on
    one binary. Canonical bytes come from `TypeInfo` (C-2), never `to_dom`.
13. **A mismatched type set on load is refused**, naming the types (D16.5 deferred).
14. **System exceptions are caught per task** and logged with system, world and step; the step
    continues (the runtime half of P5.4).
15. **The scheduler is a self-rescheduling job chain** on `setNotBefore` with an injectable clock.
    *Rejected:* a sim thread (P3.3). `World` is not a service (P21.2).

**Interfaces.**

```cpp
using StepId = TaggedTickId<StepTag>; using TickId = TaggedTickId<SimTickTag>;   // util/tick_id.hpp

namespace voxen::sim {
struct NewWorld { WorldInit init;   // generated: world_uid, seed, tick_rate_hz, frame_size_metres
  std::vector<TypedRecord> params;   // plugin init_section records
  std::vector<std::shared_ptr<const ecs::PrefabTemplate>> initial; assets::ContentSetRef content; };
struct FromSave { tk::container::View save; assets::ContentSetRef content; };
using InitSource = std::variant<NewWorld, FromSave>;

class VOXEN_API World {
public:
  static std::unique_ptr<World> create(svc::ServiceLocator &, WorldDesc, InitSource);
  svc::TaskHandle enqueueStep(const StepConfig &, StepInputs);   // at most one in flight
  StepInputs sealInputs(std::chrono::steady_clock::time_point);
  SnapshotRef stepBlocking(const StepConfig &, StepInputs);      // tests, runner
  SnapshotRef latest() const noexcept; SnapshotFeed &feed() noexcept;
  InputInbox &inbox() noexcept; ExternalInputRegistry &externalInputs() noexcept;
};
class VOXEN_API Snapshot {   // const all the way down (P2.7)
public:
  const StepHeader &header() const noexcept;   // step, tick, time, dt, traits, content hash
  const ecs::EntitySnapshot &entities() const noexcept;
  template<typename G> auto grid() const noexcept;      // v8g::TGridView<element of G>
  const MessageStore &messages() const noexcept;        // in flight after this step
  const StepInputs &inputs() const noexcept;
  const assets::ContentSet &contentSet() const noexcept;
  template<typename Id> const types::DenseIdTable<Id> &registry() const;
  std::optional<uint64_t> lastConsumedInputSeq(PlayerId) const noexcept;
};
// PublishInfo: step, nominal wall time, tick period
struct PublishedPair { SnapshotRef prev, cur; PublishInfo prev_info, cur_info; };

struct SystemDesc {   // boundary record (C-3)
  UID uid; const char *name; UID phase; const UID *writes; uint32_t write_count;
  const UID *reads; uint32_t read_count; StepTraits require, exclude;
  SystemPolicy policy; Granularity granularity; void (*fn)(SystemContext &, void *); void *user; };
class VOXEN_API SystemCatalog final : public plugin::IRegistry {
public:
  void addPhase(plugin::PluginId, const PhaseDesc &);
  void addSystem(plugin::PluginId, const SystemDesc &);
  void addTransformSource(plugin::PluginId, const ecs::TransformSourceDesc &);
  void addExternalSource(plugin::PluginId, const ExternalSourceDesc &);   // instantiated per world
  void commit(const plugin::PluginHost &) override;   // after types; names both owners on conflict
};
class VOXEN_API SystemContext {   // one per task
public:
  const Snapshot &prev() const noexcept;
  template<typename T> ecs::TableView<T> read() const;   // head if T's writer phase came earlier
  template<typename T> ecs::TableWriter<T> write();      // asserts ownership
  template<typename G> auto writeGrid();                 // v8g::TGridWriter<element of G>
  InboxView inbox(ecs::EntityKey) const noexcept;        // due messages + this step's commands
  const StepInputs &inputs() const noexcept;
  template<typename T> const T &sessionValue() const;    // world-entity component
  template<typename M> void send(ecs::EntityKey from, ecs::EntityKey to, const M &);   // at i+1
  ecs::StructuralRecorder &spawner();
  DeterministicRng rng(ecs::EntityKey) const noexcept;
};
struct InputFrame { uint64_t input_seq; ButtonSet held; std::span<const std::byte> payload;
  std::span<const PendingEdge> edges; std::span<const std::byte> edge_payloads;
  StepId presented_step; uint16_t presented_alpha; };
class VOXEN_API InputInbox {   // thread-safe producers
public:
  void submitFrame(PlayerId, const InputFrame &);
  uint32_t submitCommand(PlayerId, const CommandPayload &, WallTime);
  void submitJoin(PlayerId);                                     // repeated join: no-op
  void submitSessionValues(types::TypeId, std::span<const std::byte>);
};
class IExternalInputSource {
public:
  virtual svc::CoroSubTask<void> collect(const Snapshot &latest, ExternalInputSink &,
    const CollectOptions &) = 0;
  virtual svc::CoroSubTask<std::expected<std::shared_ptr<const void>, std::error_condition>>
  rematerialize(const InputEntryView &);   // land implements it; default NotSupported
};
class IStepHooks { public:
  virtual svc::CoroSubTask<void> beforeSeal(World &, const Snapshot &latest) = 0; };
class VOXEN_API StepScheduler { public:
  StepScheduler(World &, svc::TaskService &, Config, IStepHooks *, const IClock * = nullptr);
  void start(); void stop(); void setRunMode(RunMode); };
namespace StateHash {
VOXEN_API StateDigest compute(const Snapshot &, const types::TypeRegistry &, HashScope);
}
namespace WorldSave {   // Background; reads only the immutable snapshot (D16.2a)
VOXEN_API svc::CoroFuture<std::vector<std::byte>> write(SnapshotRef, const types::TypeRegistry &,
  tk::dom::Value header_extra);
VOXEN_API void dumpText(const tk::container::View &, const types::TypeRegistry &, std::string &out);
}
}
```

Step order (the contract with ecs and v8g):

1. **Prologue**, serial on the step job: free released snapshots and `Store::reclaim()`;
   `beginStep`; apply `AssetVersion` entries (`contentSet->with(...)`, registry allocation,
   `content_set_hash`).
2. **Phases**: anchors `PHASE_INPUT` (engine: `sim.CommandInjection`, `sim.PlayerSystem`,
   `sim.SessionApply`), `PHASE_CONTROL`, `PHASE_TRANSFORM` (`sim.TransformWrite`),
   `PHASE_GAMEPLAY` (default), `PHASE_APPLY` (single-writer appliers); plugins add phases between.
   A type is sealed at its writer phase's join.
3. **Barrier**: merge outboxes with undelivered records; `finishStep`; spawn reports become
   `sim.Spawned` records; records to despawned keys are dropped.
4. **Store publish**, then the **post-boundary group**.
5. **Assemble and publish** the `sim::Snapshot` to the ring and the feed.

`StepInputs` layout (little-endian, 8-byte aligned; the same bytes in memory, in recordings, later
in replay files and on the wire):

```
InputsHeader { u32 magic 'VXIN'; u16 version; u16 flags; u32 entry_count; u32 byte_size; i64 step;
               i64 tick; u64 action_layout_hash[2]; u32 type_count; u32 pad; }
TypeRow[type_count] { u8 uid[16]; u8 schema_hash[16]; }          referenced types, by UID
Entry { u16 kind; u16 flags; u32 size; u32 player; u32 seq; u32 type_index; u32 external; }
      + payload
  kinds 1 ActionFrame, 2 Command, 3 PlayerJoin, 4 PlayerLeave, 5 SessionValues, 6 Admission,
        7 AssetVersion, 8 InitRecord; sorted by (kind, player, seq)
ActionFrame { u64 input_seq; i64 presented_step; u16 presented_alpha; u16 edge_count;
              u16 payload_size; u16 pad; u64 held[2]; payload;
              Edge[] { u16 action; u16 fraction; u8 pressed; u8 has_payload; u16 pad };
              edge payloads }
```

Sealing merges several frames of one player (last held set and payload; edges concatenated with
`fraction = clamp((t_edge - t_prev_seal) / (t_seal - t_prev_seal)) * 65535`); a player with no new
frame repeats its held set and payload with no edges.

Save file `.vxsave` (`tk::container`, magic `VXSV`): `HEAD` (JSON DOM header: `$type`, `$version`,
world UID, step, tick, sim time, content-set hash, schema-set hash, plugin set with UIDs and
versions, `history: "empty"`, project fields; readable without the payload), `TYPE` (the C-1
table), `STEP` (`StepHeader`), `REGY` (registries), `CSET` (content-set keys), `ENTS` (entities),
one `GRID` per grid type with the type UID as chunk id (owner key plus v8g payload), `BUS`. Strict
covers the header, content-set hash, registries, strict entity fields, strict grids and strict
message types; Full covers everything persisted. Sections hash in parallel and combine in order,
so a mismatch names its section.

Scenario document (codegen `document` kind `sim.Scenario`, human-owned):

```json5
{ $type: "sim.Scenario", $version: 1, plugins: ["land", "game"],
  world: { seed: 42, tick_rate_hz: 100, params: { game: { spawn_block: [-2000, 334, -1467] } } },
  admission: { policy: "deterministic", delay_steps: 0 },   // passed to every source
  players: [0], steps: 300,                                  // players join at step 1
  inputs: [   // keyed by StepId; held sets and payloads persist until the player's next entry
    { step: 4, player: 0, hold: ["game.move_forward"], payload: { orient_q: [0, 0, 0, 32767] } },
    { step: 90, player: 0, hold: [] },
    { step: 92, player: 0, commands: [{ type: "game.PlaceBlock",
        value: { block: [-1990, 330, -1467], id: "<stone uid>" } }] } ],
  settings: [{ step: 120, set: { "game.fly.forward_mm_per_s": 200000 } }],
  asset_events: [],   // { step, asset: uid | vpath, override: {...} }
  save_reload: { at: [92, 150] },
  expect: [{ at: 300, check: "land.block_at", args: { block: [-1990, 330, -1467] },
             equals: "<stone uid>" }] }
```

Commands and checks name registry entries by UID. Digests at every `expect.at` and at the end live
in the machine-owned `<scenario>.golden.json` (P7.4), written by `--update-golden`.
`ScenarioRunner::run(scenario, Env{types, schedule, services, initial content}, Options{threads,
shuffle_seed})` runs, for one thread count, the continuous run (recording every sealed
`StepInputs` with its payloads), one run per `save_reload.at` (save to memory, destroy, load,
continue), an in-process replay of the recording and a shuffled run; all must agree and match the
golden. The runner creates worlds directly, since it owns the step inputs; the world's external
sources come from the catalog as in live play.

**Rework.** `util/tick_id.hpp`: `StepId`/`TickId` replace `WorldTickId`. `world::detail::SimThread`,
`world::ControlService`, `world::State`, `world::Player`, `PlayerStateMessage`: deleted with
`world/` in the cut (the 1/100 s tick becomes the `tick_rate_hz` default). `LandService::doTick`:
dissolved by the slice.

**Work items.**

- `sim-1` Counters, step vocabulary, `sim.schema.json5` types.
- `sim-2` Input records and the inbox. Exit: every entry kind round trips; a byte-exact golden
  buffer; concurrent submitters give a buffer independent of interleaving.
- `sim-3` Catalog and schedule. Exit: UID tie-break, trait filtering, replacement, every error kind.
- `sim-4` World and executor (needs `ecs-3`). Exit: test-plugin counters correct for 1000 steps at
  1 and 4 threads; zero-time steps bump only `StepId`; a snapshot released off the step job is
  freed by the next prologue; an index sees the same step's spawns.
- `sim-5` Bus and players. Exit: ping-pong deterministic; a shuffled order gives a byte-identical
  store; a command is delivered once; join to bound avatar.
- `sim-6` `StepScheduler` (needs `rt-3`). Exit: a fake clock gives 100 steps per simulated second;
  overload stretches without a spiral; 100 Hz on the winbox keeps jitter under 2 ms.
- `sim-7` Session values, content, registries, external inputs. Exit: scale 2.0 doubles sim time; a
  new BlockDef UID gets the next id in UID order in that step.
- `sim-8` State digest. Exit: equal for states built through different spawn orders; one changed
  byte names its section.
- `sim-9` Save and load (needs `as-1`, `ecs-7`, `v8g-5`). Exit: save, load, 500 more steps equal the
  unbroken run; `readHeader` reads no payload chunk.
- `sim-10` Scenario harness, `run-scenario` and `dump-save`. Exit: engine scenarios at 1 and 4
  threads against one golden.

### 4.6 Runtime services

**Scope.**

- **One pool** (P3.3, P3.3a): `TaskPriority{Presentation, Simulation, Background}`, one shared MPMC
  ring per class (today's ring already reserves by CAS); workers scan highest first.
- **Parking**: tasks with unmet counters and coroutines blocked on a counter park in a waiter table
  in `AsyncCounterTracker`; the `local_waiting_queue` busy-spin goes; idle workers sleep on a futex.
- **Delayed tasks**: `TaskBuilder::setNotBefore` with a timer heap and one idle worker as timer
  keeper; on Windows it waits on a high-resolution waitable timer (`os::TimerWait`), because
  `WaitOnAddress` follows the ~15.6 ms system tick (`futex.cpp:70`) and cannot pace 100 Hz.
- **Exceptions** (P5.4): caught per task into the task header; `TaskHandle::failed()`; unobserved
  failures logged by name; top-level coroutine throws no longer swallowed.
- **File I/O**: `AsyncFileIoService` with owned `IoBuffer` results, per-priority queues,
  `asyncWriteFileAtomic`, one thread by default (P3.3's external-constraint exception).
- **Services**: factories from core and plugins, `startAll()` on the main thread in registration
  order, then `seal()`.
- **Composition**: `runtime::Engine` (layout, user storage, settings, plugin host, services),
  `runtime::Session` (one world, its scheduler, the between-steps hook), `runtime::SessionControl`.
- **Hooks** (P18): `Log::ScopedContext{world, step}` as a log prefix; no-op `VOXEN_INVARIANT` and
  `VOXEN_PROFILE_ZONE`.
- **Messaging fix**: one `try` per header in `MessageQueue::pollMessages`, so a throwing handler no
  longer loses the rest of the batch (`message_queue.cpp:126-131`). Nothing else in messaging
  changes.

**Decisions.**

1. **Scan-order priority on shared class rings.** *Rejected:* random per-worker rings (a
   Presentation task waits behind a busy worker); Chase-Lev deques with stealing (no MVP
   measurement asks; the choice stays inside one `pickNext`).
2. **No preemption, no reserved worker**: presentation latency is bounded by the longest sim task,
   so sim tasks stay short. *Rejected:* a reserved worker; OS thread priorities.
3. **Parking through the counter tracker**: task, timer and I/O completions wake through one path.
4. **Delayed tasks inside `TaskService`.** *Rejected:* a timer thread (P3.3).
5. **`wait()` on a worker is a bug**; the barrier is a continuation (P3.4).
6. **Services start all at once on the main thread, then seal**; the registered factories are the
   start list. *Rejected:* a start list in settings; deadlock timeouts guarding lazy starts that no
   longer happen.
7. **The between-steps work runs in sim's `beforeSeal` hook on the step job**, so headless and
   windowed sessions behave alike and no thread is added.
8. **`SessionControl` applies requests at frame boundaries**: UI hooks run inside a frame holding
   snapshot refs and the world pointer.
9. **One Engine per process at a time** (`Log` and pipe memory are process globals); D21.1's many
   worlds are many sessions.
10. **The control plane stays as it is**: the slice deletes all four current `MessagingService`
    users, and building D6.4 now would fix its shape before the tool protocol exists.

**Interfaces.**

```cpp
namespace voxen::svc {
enum class TaskPriority : uint8_t { Presentation, Simulation, Background };
class VOXEN_API TaskBuilder { public:
  TaskBuilder(TaskService &, TaskPriority);              // no default priority
  void setNotBefore(std::chrono::steady_clock::time_point) noexcept;   // combines with addWait
  void addWait(uint64_t counter); TaskHandle enqueueTaskWithHandle(CoroTask); };
class VOXEN_API AsyncFileIoService final : public IService { public:
  CoroFuture<Result<IoBuffer>> asyncRead(FilePtr, int64_t offset, size_t size /* SIZE_MAX: EOF */,
    TaskPriority);
  CoroFuture<Result<void>> asyncWriteFileAtomic(std::filesystem::path, IoBuffer, TaskPriority); };
}
namespace voxen::runtime {
class VOXEN_API Engine {
public:
  // EngineStartArgs: setLayout, setLayerText, addRegistry, addEngineRegistration (gfx loaders,
  // engine-owned, not in the plugin set), setLoadPresentationParts
  static std::unique_ptr<Engine> create(EngineStartArgs args);
  svc::SettingsDocument &settings() noexcept; svc::UserStorage &userStorage() noexcept;
  svc::ServiceLocator &services() noexcept; const types::TypeRegistry &types() const noexcept;
  const plugin::PluginHost &plugins() const noexcept;
  sim::WorldDesc worldDesc() const;                // from the committed catalogs
  Session &createSession(SessionDesc);             // SessionDesc{InitSource, local players, paused}
  void destroySession(Session &);                  // stop, drain the in-flight step, destroy
  void requestExit(int code) noexcept; bool exitRequested() const noexcept;
};
class VOXEN_API Session final : sim::IStepHooks { public:
  sim::World &world() noexcept; sim::PlayerId localPlayer(uint32_t viewer = 0) const noexcept;
  ecs::EntityKey avatarOf(sim::PlayerId, const sim::Snapshot &) const noexcept;   // PlayerControl
  void setPaused(bool); };
class VOXEN_API SessionControl { public:   // menu-facing, main thread
  Session *active() noexcept; uint64_t generation() const noexcept;
  void requestNewGame(); void requestEndSession(); void requestExit(int code = 0);
  void setPaused(bool);
  void onBeforeDestroy(std::function<void()>);   // presentation drops its refs first
  void applyPending(); };
}
```

The between-steps hook (`Session::beforeSeal`, never concurrent with itself):

1. `AssetService::poll()`.
2. `collectAll(latest)` over `runtime::AssetInputSource` (drains `drainSimChanges`, skips hashes
   already in `latest.contentSet()`, posts `AssetVersion` entries with the `AssetRef<>` as payload)
   and the world's plugin sources (land admission).
3. If the settings generation changed, each `SessionValues` record whose bytes changed goes to
   `submitSessionValues`.
4. Once after creation, `submitJoin` for each local player.

**Startup** (`Engine::create`): `InstallLayout` (from the core DSO, or a test's `fromRoot`);
`UserStorage`; `SettingsDocument` pass 1 (platform file from `settings_dir`,
`user/<profile>/settings.toml`, core and gfx sections, `--set`); `PluginHost` with `engine.plugins`
and the registries in order (types first, because the catalog's commit needs frozen types), then
`loadAll`; settings pass 2 (plugin sections, `input.bindings` defaults; unknown sections warn,
since headless skips presentation parts); core service factories (counter tracker, pipe memory,
tasks, messaging, file I/O, `AssetService` over the committed catalog), then plugin factories;
`startAll`; `seal`. A failed load throws naming the plugin. Teardown runs in reverse.

Runtime-owned settings: `tasks.num_threads` (0 = cores - 1, at least 2), `io.threads` (1),
`engine.plugins`, `log.level`. CLI: `runtime::CommonCli` (`--root`, `--profile`, `--plugin-dir`,
`--set`, the schema `cli` aliases) for both mains; `voxen-headless run-scenario [--threads k]
[--plugin-dir d]... [--update-golden] [--report json] FILE | dump-save FILE | list-plugins |
--version`, exit codes 0 pass, 1 expectation failed, 2 usage or configuration error, 3 exception;
`--threads k` and the scenario's `plugins` become an in-memory settings layer. Test helper, one
signature: `test::createEngine(EngineOptions{plugins, settings_toml, extra_plugin_dirs, user_dir})`.

**Rework.** `svc/engine.*` moves to `runtime/engine.*` (`AppInfo` along; `makeCliParser` and
`patchConfig` go). `svc/task_*`: class rings, overflow list, futex sleep, timer keeper, waiter
wakeups, `exception_ptr`, priority and name in the header; the 11 call sites choose priorities;
`STD_THREAD_COUNT_OFFSET` becomes 1 once the world thread is gone (M3). `task_coro.*`: exceptions
surfaced, `blockOnCounter` parks, `blockingGet` added. `service_locator.*`: `startAll`, `seal`,
owner tag. `async_file_io_service.*`: reshaped plus `io_buffer.*`; `registerFile()` and the
caller-buffer read go. `pipe_memory_allocator.*`: kept with its GC thread.
`tests/voxen_test_common.*`: `test::createEngine`.

**Work items.**

- `rt-1` Messaging fix, `ScopedContext`, macros. Exit: a throwing handler in a batch of 10 still
  delivers 9.
- `rt-2` Scheduler I (rings, priorities, exceptions, stats). Exit: old task tests at 1, 2 and 8
  workers; a Presentation task starts within one task length behind 1000 queued Simulation tasks.
- `rt-3` Scheduler II (parking, `setNotBefore`, `os::TimerWait`, shutdown cancellation). Exit: a
  coroutine awaiting a 500 ms delay costs under 20 ms CPU; delayed tasks never fire early and fire
  within 2 ms on Linux and the winbox.
- `rt-4` File I/O. Exit: a Presentation read overtakes 50 queued Background reads; an atomic write
  survives a concurrent reader.
- `rt-5` Service start and seal. Exit: recursive dependencies start in order; a request after seal
  throws.
- `rt-6` Runtime module and Engine (needs `build-4`, `build-6`, `cg-10`, `as-2`). Exit: a `fromRoot`
  Engine touches no files; a static plugin's service and section work; `--set
  tasks.num_threads=3` reaches `workerCount()`.
- `rt-7` Sessions (needs `sim-6`, `sim-7`, `sim-9`, `as-4`). Exit: create, step and destroy three
  times; two concurrent sessions; a `sim_input` change appears in the next step's inputs only; an
  asset hot reload of a sim-consumed type appears as an `AssetVersion` entry.

### 4.7 Presentation

**Scope.**

- **Frame driver** `client::PresentationRuntime` on the OS main thread, replacing
  `MainThreadService`. Per frame: poll window events; route them through the UI first (S10.2); map
  input per viewer and cut a sample; submit it to `session->world().inbox()`; take
  `feed().latest()` and hand a new `cur` to snapshot consumers; compute the alpha; run the plugins'
  frame hooks; `ui.endFrame()`; build a `gfx::FramePacket`; `Renderer::drawFrame`. Without a
  session (the main menu) it skips the session steps and calls `AssetService::poll()` itself.
- **Input** (P15.1, P15.1a): actions from the `actions` kind with a domain; bindings in
  `input.bindings` (`live`), defaults from the schema; one `InputMapper` per viewer; every window
  event carries its OS time. The camera orientation enters the record through the game's payload
  struct, so mouse look is never tick-rate-limited (D10.1a's cheap form).
- **Interpolation with epochs** (S1.11, D10.1b): the published pair with nominal tick times, render
  time `now - delay`, alpha clamped to [0, 1]; an epoch change or a spawn is a cut.
- **GfxSystem split** (D10.4): `gfx::GpuContext` (device, queues, allocators, DMA, pipeline cache,
  frame ticks; creatable without a window) and `gfx::Renderer` (swapchain, graph, pipelines, mesh
  streamer, features). The `gfx -> client` and `gfx -> land/world` edges go.
- **Neutral declaration facade** (S10.3 hook): `IRenderFeature::declare(gfx::GraphBuilder &)` with
  neutral usage kinds and `gfx::Format`, forwarding to the kept `vk::RenderGraphBuilder`; pass
  bodies record through `PassContext::vk()`, each use a marked porting site.
- **Shaders as assets** (D10.5, D8.4a): `gfx::ShaderAsset` (`shader.spirv`); `PipelineRegistry`
  rebuilds on a Background task when a stamp changes and keeps the old pipeline bound meanwhile.
  Per-pipeline layouts (push descriptors) and BDA stay.
- **UI** in `voxen::ui`: event-driven input, a consumption mask per event with modal capture, stable
  ids (`tk::hash`), shaped text.
- **Text** (P20.2, D20.3 skeleton): one raw `text::FontAsset` face
  (`/core/fonts/notosans-wdth-wght.ttf`: Latin, Greek, Cyrillic) through FreeType and HarfBuzz;
  per-run shaping; greedy breaking; a layout cache keyed by (text hash, size, width, scale,
  locale); glyphs rasterized on demand into R8 atlas pages, no eviction.
- **FIFO present only**; acquire stops pumping events and reports "not available" while minimized.

**Decisions.**

1. **The frame driver is a plain object owned by the launcher**, not a service.
2. **Presentation timestamps edges; the sim's seal computes fractions** (P15.1a): local play, a
   server and replays share one path.
3. **Orientation is a field of the game's payload struct**, not an engine `ViewOrientation`.
4. **The one `ActionTable` index space**: `input::ActionSet` wraps it without re-sorting.
   Presentation-domain actions never enter records; the place click becomes a command whose target
   the game's raycast resolves against the snapshot (P15.1a). *Rejected:* per-set generated indices
   keyed by the set's `TypeId` (section 7).
5. **UI routing runs on the previous frame's layout, before mapping**: today a click on a menu
   button also toggles pause.
6. **A 1.25-tick interpolation delay, nominal times, no extrapolation.** *Rejected:* a ring of N
   snapshots (retention without visible gain at 100 Hz).
7. **A neutral declaration facade, Vulkan recording through `vk()`**: declarations are what plugin
   features write most and they survive S10.3; mods are peers (P5.5), so a Vulkan-typed plugin API
   would make every mod a porting site. *Rejected:* exposing `vk::RenderGraphBuilder`; building the
   neutral graph now.
8. **Per-pipeline layouts and BDA stay**: the winbox AMD driver runs today's shaders (land addresses
   as `uint64_t`, bug report 01, `cb61825d`); bindless needs feature checks `vk_device.cpp` does not
   make (lines 442, 518).
9. **Shaping now, one face**: the expensive interface to retrofit is the UI's text model (byte =
   glyph, fixed advance, fixed atlas). *Rejected:* keeping the baked ASCII atlas.
10. **One `IPresentationExtension` per registration**, created after the GPU exists, so a plugin's
    consumer, hook and features share state without globals.
11. **Land meshing is the slice's**; presentation provides interfaces, `MeshStreamer` and the
    facade, and no LOD code.
12. **Legacy client and gfx code is deleted at one cut, not adapted** (`pr-2`), after tagging
    `legacy-demo`; the driver runs without a session, so `voxen` stays windowed through the cut.

**Interfaces.**

```cpp
namespace voxen::input {
class InputMapper {   // per viewer; main thread
public:
  InputMapper(const ActionSet &, BindingTable, const InputSettings &, uint32_t viewer);
  void processEvents(std::span<const window::WindowEvent>, std::span<const bool> consumed);
  void releaseAll(WallTime);                          // focus loss, menu opened
  std::span<const uint16_t> pressedThisFrame() const noexcept;    // presentation-domain edges
  template<typename P> P &payloadAs();                // the declared payload struct
  template<typename C> void addCommand(const C &, WallTime);
  // An InputFrame for sim::InputInbox plus the commands; nullopt when there is nothing to send
  std::optional<InputSample> cutSample(WallTime now, sim::StepId presented, uint16_t alpha);
};
}
namespace voxen::text {
struct PositionedGlyph { uint32_t glyph_index; uint16_t face /* 0 */; uint16_t line; float x, y;
  uint32_t cluster; };   // cluster = byte offset into the UTF-8 source
struct TextLayout { std::vector<PositionedGlyph> glyphs;
  std::vector<TextRun> runs;   // script, bidi level (0 in the MVP)
  m::float2 size; float baseline; };
class LayoutCache { public:
  const TextLayout &layout(std::u8string_view, TextStyle, float max_width, float scale); };
}
namespace voxen::gfx {
class VOXEN_GFX_API GraphBuilder {   // valid during declare(); forwards to vk
public:
  ViewHandle output();
  ImageHandle makeImage(std::string_view name, const ImageDesc &);   // gfx::Format
  BufferHandle makeBuffer(std::string_view name, const BufferDesc &);
  void publish(std::string_view name, ViewHandle);   // shared by name: "scene.depth"
  std::optional<ViewHandle> find(std::string_view) const;
  template<auto MemFn, typename F> void renderPass(std::string_view, F &self,
    std::span<const ColorTarget>, std::optional<DepthTarget>,
    std::span<const ResourceUse> uses);   // a UsageKind per use
};
class VOXEN_GFX_API PassContext { public: const FramePacket &packet() const noexcept;
  vk::PassContextVk &vk() noexcept; };   // BACKEND-SPECIFIC, an S10.3 porting site
class IRenderFeature { public:
  virtual std::string_view id() const noexcept = 0;
  virtual void createPipelines(PipelineRegistry &, Format output, Format depth) = 0;
  virtual void declare(GraphBuilder &) = 0;
  virtual void prepare(const FramePacket &, PrepareContext &) {} };
class VOXEN_GFX_API PipelineRegistry { public:
  PipelineId add(const GraphicsPipelineDesc &);   // neutral state; shaders by vpath
  bool ready(PipelineId) const noexcept; void update(); };   // polls Asset<ShaderAsset>::stamp()
}
namespace voxen::client {
class ISnapshotConsumer { public:
  virtual void onSnapshot(const sim::Snapshot *older, const sim::Snapshot &newer,
    svc::TaskService &) = 0;
  virtual void sessionEnding() noexcept {} };
class IFrameHook { public: virtual void onInput(FrameContext &) {}
  virtual void buildScene(FrameContext &) {}
  virtual void buildUi(FrameContext &, ui::UiBuilder &) {} };
class VOXEN_GFX_API PresentationRegistry final : public plugin::IRegistry { public:
  void addExtension(plugin::PluginId, UID uid, Factory);   // same uid later replaces (D5.6)
  void commit(const plugin::PluginHost &) override; };
// before == null (spawned) or a changed epoch: cut, use `now`
InterpolatedTransform interpolate(const ecs::Transform *before, const ecs::Transform &now,
  float alpha, float frame_size_m) noexcept;
}
```

Settings sections (one schema per module): `window`, `gfx` (today's three switches),
`gfx.renderer` (`features` order, `present_mode: "fifo"`), `present` (interpolation delay, FPS
logging; live), `input` (mouse sensitivity, `bindings: map<string, array<string>>`; live), `ui`
(scale live, font vpath). Session binding: on a `SessionControl` generation change and in
`onBeforeDestroy`, the driver drops its `SnapshotPair`, calls `sessionEnding()` and releases all
mappers, so no ref outlives its world (C-5). The local avatar is `session->avatarOf(localPlayer(v),
*cur)`, read through `entities().get<ecs::Transform>(key)` on `prev` and `cur`.

**Rework.** Cut in `pr-2`: `client/main_thread_service.*`, `gameview.*`, `gui.*`,
`input_event_adapter.*`, `player_action_events.hpp`, `configs/action_settings.toml`,
`gfx/gfx_system.*`, `gfx_land_loader.*`, `gfx_land_structs.json5`, `font_renderer.*`,
`gfx/vk/legacy_render_graph.*`, `standard_render_graph.*`, `gfx/graph/*`, `src/game/*`. Their
survivors: camera matrices in `gfx/camera.cpp`; the UI pass in `gfx/features/ui_feature.cpp`; the
selector cube in `features/debug_draw_feature.cpp`; land passes and LOD logic in the land plugin,
ported from history. `gfx/ui/*` moves to `ui/` (stable ids replace the `std::hash` at
`ui_system.cpp:38`). `gfx/vk/render_graph_*`: callback context, swapchain passed in, graph-tracked
transient buffers. `vk_pipeline.*`, `vk_pipeline_layout.*`, `vk_descriptor_set_layout.*`: replaced
by `PipelineRegistry`. `vk_swapchain.*`: non-blocking acquire. `vk_instance.*`, `vk_device.*`:
extensions passed in, swapchain optional. `vk_pipeline_cache.*` (path from `UserStorage`),
`vk_mesh_streamer.*` (wrapped), `frame_tick_*` (owned by `GpuContext`): kept. `src/shaders/land/*`
are copied into the land plugin; `selector.slang` becomes `src/shaders/debug/debug_lines.slang`.

**Work items.**

- `pr-1` Text stack, additive. Exit: "Привет, world" shapes with monotonic x and clusters mapping
  to byte offsets; greedy breaking; cache hits; a full atlas returns nullopt.
- `pr-2` **The gfx cut** (needs `build-3`, `build-6`, `build-7`, `rt-6`; lands with `cg-10`'s
  deletions and the deletion of `world/`): tag and delete the files above; window reshape and key
  names; `GpuContext`/`Renderer`, swapchain change, graph reshape, the facade; the UI move with
  routing and the UI and debug-draw features; `PresentationRegistry`, `PresentationRuntime`
  without a session; the debug overlay. Exit: `voxen` with no plugins shows the overlay on Linux
  and the winbox (Intel, AMD); minimizing idles; CPU tests for key names, event times, UI
  press-and-release and capture, the usage table and shared `scene.depth`.
- `pr-3` `PipelineRegistry` with hot reload (needs `as-3`..`as-6`). Exit: editing `ui_basic.slang`
  and building `voxen-data` changes the running `voxen`; a broken shader keeps the old pipeline.
- `pr-4` The remaining presentation settings sections (`gfx` exists since `cg-10`). Exit:
  `--set present.interpolation_delay_ticks=2` and a user-layer binding take effect.
- `pr-5` Input module. Exit: binding diagnostics; consumed events never press but a release is
  always honoured; `cutSample` is empty when idle; focus loss releases everything.
- `pr-6` Frame driver with sessions (needs `sim-4`, `rt-7`). Exit: with a stub game plugin the
  camera moves through input records; CPU tests for alpha clamping, epoch cuts, the sealed
  `StepInputs` of a submitted sample, and no live `SnapshotRef` after `reset()`.
- `pr-7` Slice support: interface fixes for `sl-7`; the land feature's `vk()` call sites recorded
  as the S10.3 porting inventory.

### 4.8 Plugins and the slice

#### Plugin contract

**Scope.** One JSON5 manifest per plugin, at most two fixed-name part DSOs, one exported C entry
per part, a typed registrar over registries other pieces own (staged, then committed), and
`plugin::PluginHost`, owned by `runtime::Engine`. No hot reload: parts unload at Engine destruction
in reverse load order.

**Decisions.**

1. **One manifest, at most two parts** (`<id>.so`, `<id>-gfx.so`): the mod set in saves and
   handshakes is per plugin (D16.5, D12.5), headless skips the presentation part and records the
   same set, and fixed names avoid Windows DLL name collisions. *Rejected:* `land-gfx` as a
   separate plugin; a second entry symbol for presentation.
2. **The entry is an exported `extern "C"` function returning a standard-layout descriptor**; the
   ABI check is one integer (P11.2 promises no ABI). *Rejected:* an exported data symbol (fragile
   on Windows, llvm#87887); an engine schema hash in the header (the type table check covers it).
3. **A typed-registry registrar** with `beginPart` owner tagging and one `commit` per registry;
   providers keep their method names; D5.6 conflicts surface at startup naming both owners.
   *Rejected:* a fat `Registrar::addX` per concept; UID-keyed facets.
4. **Load order is the settings list; requirements must come earlier**; violations are errors.
5. **Plugins never link other plugins**: `USES <id>` gives headers (layouts, UID constants, inline
   readers), out-of-line functions are reached through registries, `--no-undefined` enforces it.
6. **`plugins/<id>/`, `<id/...>` includes, namespace `id::`** (`land`, `land::gfx`, `game`): they
   look like mods (P5.5) and cannot clash with legacy `include/voxen/land/`.
7. **Plugin unit tests compile the part's OBJECT library**: a test in `bin/` would not find
   `plugins/land/land.dll` on Windows.
8. **The plugin list is a plain array**, `engine.plugins = ["land", "game"]`, replace policy.
   *Rejected:* a keyed `{id, enabled}` list (needs the deferred `keyed` merge policy).

```cpp
namespace voxen::plugin {
enum class PartKind : uint32_t { Sim = 1, Presentation = 2 };
struct PartDescriptor { uint32_t abi_version; PartKind kind; void (*register_part)(Registrar *); };
// VOXEN_PLUGIN_PART(kind, fn) -> extern "C" const PartDescriptor *voxenPluginPart() noexcept
class VOXEN_API IRegistry { public:
  virtual UID registryUid() const noexcept = 0;
  virtual void beginPart(const PluginInfo &, PartKind) {}   // owner of what follows
  virtual void commit(const PluginHost &) = 0; };          // validate, layer order, freeze
class VOXEN_API Registrar { public:   // valid during register_part only
  PluginId owner() const noexcept;
  template<CRegistry R> R &registry(); template<CRegistry R> R *findRegistry() noexcept;
  template<svc::CService S> void addService(svc::TServiceFactoryFunction<S>); };
class VOXEN_API PluginHost { public:
  explicit PluginHost(Config);   // plugin_ids, plugin_dirs, load_presentation_parts
  void addRegistry(IRegistry &);
  void addStatic(PluginManifest, std::span<const PartDescriptor *const>);   // tests, embedding
  void loadAll();                // manifests, requires order, abi check, register in order, commit
  std::span<const PluginInfo> plugins() const noexcept; };
}
```

Registries: `types::TypeRegistry` (`addModule`), `sim::SystemCatalog` (phases, systems, transform
and external sources), `assets::AssetCatalog` (loaders; packs from `plugins/<id>/pack/`),
`client::PresentationRegistry` (extensions). A part function touches no service: none exist yet.
Manifest: `{ $type: "voxen.plugin.Manifest", $version: 1, uid, id: "game", name, version,
parts: ["sim", "presentation"], requires: [{ id: "land", uid }] }`.

CMake: `voxen_add_plugin(id MANIFEST f)` creates `plugin-<id>` and INTERFACE
`plugin-<id>-headers`; `voxen_add_plugin_part(id SIM|PRESENTATION SOURCES ... [SCHEMAS ...]
[USES id...])` creates OBJECT `plugin-<id>-sim-objects`/`-gfx-objects` and SHARED
`plugin-<id>-sim`/`-gfx` (no prefix, output in the plugin dir, `INSTALL_RPATH
"$ORIGIN;$ORIGIN/../../bin"`, a generated `<ID>_API`); the presentation form is a no-op without
gfx. Windows parts open with `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR`, so `land-gfx.dll` finds
`land.dll`. `voxen_add_scenario_test(NAME n SCENARIO f THREADS 1 4)` adds `scenario:<n>:t<k>`.

- `build-6` `os::DynamicLibrary::openPath`, the `plugin` module, `VoxenPlugins.cmake`, two test
  plugins. Exit, on both OSes: commit order; a late requirement, a missing dir, an id mismatch, a
  duplicate id, a missing `$version`, a bad ABI and a wrong kind each name the plugin; headless
  skips presentation parts; `dlclose` after the registries are released.
- `build-7` `src/app/` mains (under 100 lines each). Exit: `voxen-headless --version` and
  `list-plugins` work headless; `voxen` opens a window (with `pr-2`).
- `build-8` `land` and `game` skeletons, `plugins-load`. Exit: headless loads only sim parts; a
  planted call from game into a non-inline land function fails the link.
- `build-9` Scenario ctests at 1 and 4 threads in the Linux, Windows and headless jobs.

#### The slice: land and game

**Scope.**

- **Grids** (D2.8): `land.BlockIds` (`uint16` `land::BlockId`, 32 per frame unit, one frame = one
  24 m chunk, 8^3 tiles of 1 KiB, YXZ inside a tile) and `land.ChunkState` (one `uint16` per chunk:
  `RESIDENT` and `EDITED` flags, generator identity index; absent = not resident), owned by the
  well-known `land.Terrain` in the world cell.
- **`ChunkKey`** ported into `land::` with its fields (22/16/22/4), canonicalisation (X/Z wrap), a
  44-bit Morton code and Morton order. Tile keys use `MortonLayout3` with axis bits (21, 11, 18)
  and a power-of-two bias `2^(bits-1)` declared in the grid schema, so `GridSet` and
  `land::GridKeys` share one layout and every 2^n-aligned chunk cell is one key range.
- **Generator** (D22.1): pure `(GeneratorContext{seed, identity, material ids}, ChunkKey) ->
  ChunkBlocks` with two built-in stages; identity = hashed ordered stage list plus the material id
  map; generator and geometry TUs compile with per-source `-ffp-contract=off` (pragmas do not
  reach header-inline `m::` math); golden hashes on Linux and the winbox.
- **Admission** (P15.2a): off-tick generation; a chunk enters only as a recorded
  `land.ChunkAdmit{chunk, generator_identity}` with 64 `DetachedTile`s as the payload, from
  `land::AdmissionSource`. The wanted set (radius 3 around `land.ResidencyAnchor` entities, from
  `Transform.position.frame`) is a pure function of the snapshot (D16.3). No drops.
- **Edits** (P1.13, D2.9): `BlockGridSystem` (`PHASE_APPLY`) is the single writer of both grids:
  admissions, then `land.SetBlock` in delivery order via `mutableTile` and `publishTileIfChanged`,
  then `EDITED` on real changes.
- **Registry and definitions**: `land.BlockTypes` over sim-consumed `land.BlockDef` assets
  (`display_name`, `color`, `placeable`, `solid`, `opaque`); `AIR` reserved, `MISSING` placeholder.
- **Meshing** (P2.4): `land::lod::LandMesher` in the land sim DSO, CPU-only, headless-tested, run on
  the presentation timeline: diffs both grids between presented snapshots and reuses today's DAG
  with aggregation; non-resident regions come from its own generator cache (pristine, since nothing
  is dropped); mesh versions are a mesher-local counter.
- **Game, sim part**: archetype `game.Avatar` (`Transform`, fixed-point `AvatarMotion` with an
  `epoch`, `FlyController`, `land.ResidencyAnchor`) in `game.Players`. `game.init` (Init trait;
  `game.WorldRules` from the `WorldSetup` record); `game.spawn` (`PHASE_CONTROL`; for each player
  whose `PlayerControl.avatar` is null, read from the head after `sim.PlayerSystem`, spawns an
  avatar with origin = the player entity and `notifyOrigin(POSSESS)`); `game.controller`
  (`PHASE_CONTROL`; owns `AvatarMotion` and `FlyController`; `FlyMath::step` from the record and
  `FlyRulesSessionValues`); `game.commands` (`PHASE_GAMEPLAY`; validates `PlaceBlock` for reach,
  residency, a registered id and `placeable`, then sends `land.SetBlock`). The source
  `game.FlySource` derives `Transform` exactly.
- **Presentation parts**: `land::gfx::LandPresenter` (consumer and hook: feeds the mesher, uploads
  by (chunk, version)), `LandRenderFeature` (cull compute and draw), `BlockMaterialTable` (colours
  from the registry and `contentSet()`, magenta for missing definitions, read by `chunk_mesh.slang`,
  resolving its TODO at line 48); `game::gfx::FlyCamera` (mouse look and roll into the payload),
  `GameHud` (digit hotbar, selector, a `PlaceBlock` 5 m along the view), `GameMenus` (Single
  player, Exit; Resume, Exit).

**Decisions.**

1. **`ChunkKey` keeps its bitfields**; Morton codes span the canonical range (44 bits per chunk, 50
   per tile). *Rejected:* narrower fields; keys over 64 bits (breaks D2.8).
2. **Residency is a grid**: per-cell ownership, saving and diffing come free (D3.2b).
3. **The admission payload is attached, not recomputed in the tick.** *Rejected:* in-tick
   generation; recording tiles.
4. **No residency system, no drops**: the wanted set is derived until drops need state.
5. **Small sim radius (3), large render interest (LOD0 radius 8)**: render reads pristine regions
   from the generator, so sim, save and hash stay small.
6. **Edits go avatar -> `SetBlock` -> grid at i+1**, exercising S1.3: the save at 92 lands between
   validation and apply. *Rejected:* a same-step phase read (D1.14).
7. **Fixed-point controller**: every value the slice decides on is strict (`AvatarMotion`, grids,
   `TerrainInfo`); no system reads `Transform` in the tick, and residency uses only the strict
   frame (S1.4a).
8. **Tunables are layered documents** (D5.6): speeds in `game.fly`, reach in `game.build` (both
   `sim_input`, fixed point), the spawn in the world document; only `MAX_ADMISSIONS_PER_STEP` is a
   constant (P1.9a).
9. **Two generator caches (admission, mesher) over one memoized `GeneratorContext`**: no tile
   crosses between sim and presentation; mesher tasks hold a `SnapshotRef`; generated data is the
   land value `ChunkBlocks` (uniform or `shared_ptr` dense tiles), never a v8g page.
10. **Port, do not move**, with characterization tests against the legacy classes, which stay until
    M8.

**Interfaces.**

```cpp
namespace land {
class ChunkBlocks {   // immutable value, safe across threads
public:
  static ChunkBlocks fromDense(const CubeArray<BlockId, 32> &);
  static ChunkBlocks fromGrid(const v8g::TGridView<BlockId> &, ChunkKey);   // snapshot held
  void encodeTiles(v8g::PageAllocator &, const v8g::GridLayout &,
    std::span<v8g::DetachedTile, 64> out) const;   // admission payload
};
namespace Generator {   // pure, thread-safe, strict
ChunkBlocks generateChunk(const GeneratorContext &, ChunkKey);
void generatePseudoChunk(const GeneratorContext &, ChunkKey, lod::PseudoChunkData &out);
}
class TerrainView {   // header-only; usable by game without linking land
public:
  explicit TerrainView(const sim::Snapshot &) noexcept;
  explicit TerrainView(const sim::SystemContext &) noexcept;
  BlockId blockAt(BlockCoord) const noexcept;   // AIR if not resident
  bool isResident(ChunkKey) const noexcept;
  const BlockDef *blockDef(BlockId) const noexcept;   // registry uid -> contentSet().get
};
namespace lod { class LAND_API LandMesher { public:
  void update(sim::SnapshotRef now, ChunkKey viewpoint);   // diff, then enqueue the DAG
  void drainCompleted(
    extras::function_ref<void(ChunkKey, uint64_t version, const PseudoChunkSurface &)>); }; }
}
```

```json5
// plugins/game/schema/game.schema.json5 (abridged)
{ schema_language: 1, module: "game", namespace: "game", uid: "…", tier: "strict",
  imports: ["land", "voxen.ecs"],
  types: {
    AvatarMotion: { kind: "component", uid: "…", version: 1,   // pos_q in 1/4096 block units
                    fields: { pos_q: "int64x3", orient_q: "int16x4", epoch: "uint32" } },
    Avatar:      { kind: "archetype", uid: "…", cell_rule: { kind: "logical", cell: "Players" },
                   components: ["voxen.ecs.Transform", "AvatarMotion", "FlyController",
                                "land.ResidencyAnchor"] },
    AvatarInput: { kind: "struct", uid: "…", version: 1, fields: { orient_q: "int16x4" } },
    FlyActions:  { kind: "actions", uid: "…", payload: "AvatarInput", actions: {
                   move_forward: { uid: "…", domain: "sim", bindings: ["Key:W", "Key:Up"] },
                   place:        { uid: "…", domain: "presentation", bindings: ["Mouse:Left"] } } },
    PlaceBlock:  { kind: "command", uid: "…", version: 1, predictable: true,
                   fields: { block: "int32x3", id: "ref<registry:land.BlockTypes>" } },
    FlyRules:    { kind: "settings", uid: "…", version: 1, path: "game.fly",
                   fields: { forward_mm_per_s: { type: "int32", default: 100000,
                                                 sim_input: true } } },
    WorldSetup:  { kind: "struct", uid: "…", version: 1, init_section: "game",
                   fields: { spawn_block: { type: "int32x3", default: [-2000, 334, -1467] } } },
  } }
```

`init_section` marks a fixed-layout struct as a plugin's section of the world document: runtime
converts it through `from_dom` into a `NewWorld::params` record, and Init-trait systems read it
with `ctx.inputs().initRecord<game::WorldSetup>()`.

**Rework.** One destination per legacy file: `chunk_key`, `land_consts`, `cube_array`,
`land_utils` are copied into `plugins/land/include/land/`; `compressed_chunk_storage` and
`land_chunk` become `ChunkBlocks`/`ChunkStencil`; `land_generator` becomes `GeneratorContext` and
`Generator::` (ids, not colours; the tick hooks and regional map dropped); `pseudo_chunk_*` and
`land_geometry_utils*` go to `plugins/land/src/sim/lod/`; `land_service` dissolves (DAG and LRU into
`LandMesher`, `editBlock` into `BlockGridSystem`, `doTick` admission into `AdmissionSource`);
`chunk_ticket`, `land_messages*`, `land_state`, `land_temp_blocks` are deleted; `client/gameview`
splits into `FlyMath`, `FlyCamera`, `GameHud` and `GameMenus`; `src/game/game_ui*` is ported into
`game_menus.cpp` without Multiplayer and Settings; `src/shaders/land/*` gain the material lookup
(BDA kept) and lose the packed-555 colour path; `tests/voxen/land/*` are ported into
`plugins/land/tests`.

**Work items.**

- `sl-1` Keys and tiles. Exit: contiguity of scale-n ranges (n = 0..9); wrap; `ChunkStencil`
  byte-equal to legacy `ChunkAdjacencyRef`.
- `sl-2` Pure generator. Exit: golden hashes (16 chunks, 4 pseudo chunks) on both OSes; at least
  99.9% equal ids against the legacy generator, which contracts FMAs.
- `sl-3` Land sim part. Exit: a headless world reaches a full resident box; a no-op `SetBlock` gives
  an empty diff and no findings; A->B->A in one step publishes nothing; deterministic admission is
  independent of thread count; rematerialized payloads equal the originals.
- `sl-4` Game sim part and `fly_place`. Exit: section 1; `FlyMath` exact.
- `sl-5` Block definitions as assets and `block_def_reload`. Exit: section 1.
- `sl-6` `LandMesher` port. Exit: one edit invalidates the 6 neighbour surfaces and 8 parent
  entries; an unchanged snapshot enqueues nothing; surfaces equal legacy `PseudoChunkSurface`.
- `sl-7` Presentation parts. Exit: parity; a colour change rebuilds only the material table.
- `sl-8` GPU hot-reload checks (section 1).

## 5. Milestones

Each milestone lands as commit series that build and pass all tests on the Linux, Windows (winbox
recipe) and headless jobs. Work items inside a milestone may run in parallel where their
dependencies allow; `sl-1` and `sl-2` need only a plugin-local static target and may start as
early as M1.

**The cut-over point is `pr-2` in M3.** Until then the old `game` executable runs and is edited
only mechanically (`build-3`'s include and macro changes). At `pr-2` the last runnable commit is
tagged `legacy-demo`, and `game`, the legacy client and gfx code, `world/` and the old settings
(`Config`, `gfx_runtime_options`, `GfxOptions`) are deleted in one series. From then on `voxen`
runs without a world (the debug overlay) and the slice is checked headless until M8 restores
parity. Legacy `land/` stays in core, untouched and unlinked by anything new, as the
reference for characterization tests, and is deleted in M8.

- **M0 Tree foundation.** `build-1`..`build-4`, `v8g-0`, `sim-1`, `rt-1`. Exit: behaviour
  unchanged; the headless job and the layering check run in CI; tests find their root without
  argv0. Old `game`: runs.
- **M1 Toolkit and storage primitives.** `cg-1`..`cg-8`, `build-5`, `as-1`, `v8g-1`..`v8g-5`,
  `ecs-1`, `ecs-2`, `pr-1`. Exit: schemac output compiles into `test-tk` and on the `windows-ninja`
  Multi-Config preset; v8g model tests and the grid codec round trip pass; the container round
  trips. Old `game`: runs (`svc::Config` over a thin DOM adapter).
- **M2 Scheduler and I/O.** `rt-2`..`rt-5`, `as-2`. Exit: priority, parking and timer tests,
  including 2 ms timer precision on the winbox. Old `game`: runs, with explicit priorities.
- **M3 Engine, plugins and the gfx cut.** `cg-9`, `cg-10`, `build-6`, `rt-6`, `build-7`, `pr-2`.
  Exit: `voxen` with no plugins opens a window with the debug overlay (FPS, a Cyrillic line) on
  Linux and the winbox; `voxen-headless list-plugins` lists the test plugins; the test plugin and
  core resolve one `TypeId`. Old `game`: deleted here.
- **M4 Assets and shader hot reload.** `as-3`..`as-6`, `pr-3`, `pr-4`, `pr-5`. Exit: editing
  `ui_basic.slang` and building `voxen-data` changes the running `voxen`; a user-layer binding and
  `--set` take effect; a headless configure never runs slangc. `voxen`: overlay only.
- **M5 ECS and sim core.** `ecs-3`..`ecs-9`, `sim-2`..`sim-10`, `v8g-6`, `v8g-7`. Exit:
  `scenario:empty` at 1 and 4 threads against one golden in every job; save, load and 500 more
  steps equal the unbroken run; the P2.6 results table updated. `voxen`: overlay only.
- **M6 Sessions and the frame driver.** `rt-7`, `pr-6`, `as-7`, `build-8`. Exit: both binaries load
  both plugin skeletons; with a stub game plugin, input records move the camera; an asset hot
  reload of a sim-consumed type appears as an `AssetVersion` entry. `voxen`: overlay only.
- **M7 The slice, sim side: the headless acceptance gate.** `sl-1`..`sl-5`, `cg-11`, `build-9`.
  Exit: `scenario:fly_place` and `scenario:block_def_reload` pass at 1 and 4 threads in the Linux,
  Windows and headless jobs, with the same goldens on both OSes. `voxen`: overlay only.
- **M8 Parity and cleanup.** `sl-6`..`sl-8`, `pr-7`, `build-10` (delete legacy `land/`, the
  layering allow-list and the hash trie's last users; finish `architecture.md` and
  `conventions.md`; move the deferred CI rows to `todo.md`). Exit: the manual parity and hot-reload
  checks of section 1; the allow-list is empty; no legacy `voxen::land` symbol remains. `voxen`:
  full parity. The MVP is done here.

## 6. Deferred

Grouped by draft section. The reason is "no slice consumer" unless stated; each entry names the
hook that keeps it possible.

**1. Simulation model**

- Periodic and reactive policies, timers (P1.7): every slice system is an always-task. Hook:
  `SystemPolicy` keeps all values; `MessageRecord::deliver_tick` is separate from `send_tick`.
- Fan-out granularities, v8g ranged writers, ecs partitions: no slice system fans out. Hook:
  `Granularity`; 2^16-aligned windows; `Join::forEach(SlotRange, fn)`.
- Coalescing delivery, bounce, per-cell outboxes (D6.4). Hook: reserved `MessageFlags`.
- Project trait bits, the paused-editable mode, demand-driven ticking (S1.15, D1.15a). Hook:
  reserved trait bits; zero-time steps work; `StepWhen::OnDemand`, `nextDueTick()`,
  `frameNeeded()`.
- History H > 1 (P1.1a), per-speed periods (P1.7a), taint reporting (S1.4a), phase overlap (P3.4).
  Hook: the ring takes a minimum; `history(age)`; `timeScale()`; declared reads; the phase DAG.

**2. State storage**

- Blobs (D4.3a): no variable-length slice field. Hook: null-only `copy`/`destroy` slots; one leaf
  copy function; a dedicated diagnostic for `string`/`array` in fixed layouts.
- Palette tiles, per-type leaf size, sparse storage, path compression, size-class or NUMA
  allocation, Merkle node hashes, page compression. Hook: reserved kind byte and `DirNode` fields;
  `leaf_items_log2`; size and tag passed on free; container chunk flags.
- The hash trie reshape: no consumer before legacy land goes. Hook: moved unchanged, unblocked in
  M8.
- `mprotect` of published pages (P18.4). Hook: epoch asserts on every write.

**3. Parallelism**: stealing, per-worker deques, `parallelFor`, `WorkerLocal`, task groups,
cancellation, the pipe-memory GC rework (P0.5, P3.3). Hook: one `pickNext`; unchanged `TaskBuilder`
signatures; `workerCount()`; a reserved group slot in the task header.

**4. Entities**

- Spatial cells, hierarchy and `Parent`, `Follow`/`Shard`, cell groups, dormant and runtime cells
  (D3.2a-c, S4.3). Hook: reserved rule kinds and `CellRecord` fields; the boundary resolves a target
  cell per request and expands requests before sorting.
- Entity and component diffs; incremental, lagging and lazy indices (S4.5, S4.5a). Hook: declared
  `visitComponentDiff(older, newer, fn)`; `IIndexMaintainer::update` defaulting to `rebuild`.
- Prefab `base` chains and a scene text format (S4.2, P7.3). Hook: `base` reserved and rejected.
- Variable windows, predicted keys, wrapping worlds, archetype change, quarantine (P5.4), transient
  types (D16.2a), project tier overrides (S1.4a). Hook: opaque slots; a reserved key range;
  `FramePosMath`; header flags; `persist`; tier as data in `TypeInfo`.

**5. Plugins**: hot reload (S5.2), the C ABI and C# binding (P5.1), dependency sorting, user mod
dirs, third-party generator stages (D22.1). Hook: `beginPart` owner tags; C-3; `requires` objects;
`plugin_dirs` as a list; a UID-hashed stage list and a declared `IGenStage`.

**6. Services and messaging**: the D6.4 control plane (replies, timeouts, cancellation, streams,
topics, a loopback DOM transport, DOM codecs, engine message types) and broadcast routing; no
consumer exists (P6.2, P9.3). Hook: `MESSAGE_UID`, the router and `MessagingService` stay; a future
`control` schema kind reuses the `document` emitter.

**7. Documents**: P7.6 and D4.1b converters and the corpus; merge policies `append`, `keyed`,
`$replace`; `optional`, `bytes`, non-string map keys; network, cheat and console attributes; the
menu-written settings layer, `pendingRestart`, provenance; a GPU struct emitter. Hook: `$version`
is required and saves record UID, version and schema hash; enums grow additively; unknown attributes
are rejected; `SourceRef` names file and line.

**8. Assets**: archive mounts (P11.1), locale variants (P20.1), partial levels (P8.4), the P7.2
template layer, a native watcher, unmount, shadowing warnings, authored UIDs for raw assets, a
content-hashed DAG with dependencies (P8.3), rematerialization. Hook: index-plus-ranges `IMount`;
`MountDesc::variant`; `LoadContext::readChunk()`; an ordered cook layer list; `indexStamp()`;
explicit `uid` in index records; reserved input hash and `deps`; `acquireVersion(uid, hash)`.

**9. Tools and editor** (S9.4, P9.3). Hook: zero-time steps, `Store::resetTo`, commands as inputs.

**10. Rendering and UI**

- Neutral recording, D3D12, bindless and BDA removal (S10.3). Hook: neutral declarations and a
  greppable `vk()`; the recorded precondition that `vk_device.cpp` check and enable descriptor
  indexing's individual features (runtime arrays, partially bound, update-after-bind for storage
  buffers and sampled images, update-unused-while-pending, non-uniform indexing) and limits, tested
  on the winbox Intel and AMD GPUs.
- Prediction and predicted events (D10.1a, D10.1b). Hook: recorded `input_seq` and
  `lastConsumedInputSeq`; `InputMapper::unconsumed()`; `FlyMath::step` reusable bit for bit.
- Render proxies, an offscreen target and image tests (P19.4), the graph as a document (D10.5),
  more present modes, GPU recording on pool tasks, gamepads and axes. Hook: `ISnapshotConsumer`
  and `interpolate()`; the graph imports its output; `features` order; `present_mode`; a split
  `drawFrame`; reserved `ActionKind`s.
- Save and load from the menu. Hook: `createSession(FromSave)` and `WorldSave`; a save-slot service
  (D16.2a) adds the buttons.

**11. Build**: SDK and install rules (P11.1, P11.2), sanitizer and non-PCH jobs, per-case test
discovery, a rebuild-locality check (D4.1a), more ABI fields, a sim DSO. Hook: alias targets; build
tree = install layout; `voxen_add_test_executable`; per-target `gen/`; `abi_version` first;
`ecs < sim < runtime` at the top of core.

**12. Networking**. Hook: `StepInputs` is little-endian, UID-typed and carries the action layout
hash; diffs are `(older, newer)`; plugin sets are per plugin.

**13. Audio** and **14. Physics**: out of the MVP. Hook: `Transform` sources accept any mover;
`TaskPriority::Presentation` covers audio feeding.

**15. Input**: lag compensation. Hook: the frame records `presented_step` and `presented_alpha`.

**16. Persistence**: cell journals, generator-elided pages, streaming saves (D16.2, D16.3); opaque
carry of unknown types (D16.5); the replay file, periodic digests and divergence reports (P16.4);
admission delays across a save point; residency drops. Hook: leaf epochs and `KeyRange`
everywhere; the `TYPE` chunk and `checkTypeTable`; `StepInputs` as a flat buffer and land's
`rematerialize`; `delay_steps: 0` in scenarios with save points; `ChunkState` flags and
`BlockGridSystem` as the one drop site.

**17. Memory**: budget views beyond page tags. Hook: `BudgetTag`.

**18. Observability**: log categories and sinks, the crash handler, findings, Tracy (P18.1-P18.4).
Hook: `Log::ScopedContext`; `VOXEN_INVARIANT`, `VOXEN_PROFILE_ZONE`.

**19. Testing**: 32-thread and shuffled-cell nightly runs, tracked benchmarks (P19.2, P19.3). Hook:
`voxen_add_scenario_test` thread lists and labels; `bench-v8g --format json`.

**20. Localization**: fallback chains, eviction, bidi, UAX #14, CJK, rich text, string tables.
Hook: `PositionedGlyph::face`, cluster maps, run direction, the locale cache key, a paged atlas.

**22. Remaining** (D22.2, D22.3, D10.4). Hook: a windowless `GpuContext` for compute consumers.

## 7. Decisions for the author to confirm

Chosen option first, main alternative after "vs".

1. Two engine DSOs vs core/sim/gfx.
2. One module = one directory = one library, so `window/` (`voxen::window`) vs `os::Window` built
   into gfx.
3. Plugins in `plugins/<id>/`, namespace `id::` vs a `voxen_<id>::` prefix.
4. No plugin links another (`USES` headers) vs a `REQUIRES_LINK` exception with an rpath rule.
5. A plain plugin array vs a keyed `{id, enabled}` list with a `keyed` merge policy.
6. The root from the `voxen-core` DSO location vs argv0 or the executable path.
7. `TypeId` per registry in UID order vs per-world type ids (content registries stay per world).
8. Per-field tiers with defaults vs project overrides of whole core types.
9. Required `$version` in every human-owned file vs "missing means current".
10. Field order from JSON5 key order vs arrays of `{name}`.
11. Vendored xxHash vs an own XXH3.
12. A `UInt` DOM kind vs int64 only with hex strings.
13. One action payload per plugin set vs concatenated payloads.
14. A process-wide action index with a layout hash vs per-set indices keyed by the set's `TypeId`.
15. Registries as snapshot side objects allocated in the prologue ("first use" = the first step a
    definition exists) vs a v8g table on a well-known entity (P4.8) or allocation on placement.
16. Slot order from state vs insertion order plus an order-independence contract.
17. Keys only at the boundary (avatar bound in the third step after a join) vs pending handles.
18. One engine `Transform` writer fed by sources vs a declared integrator.
19. Offsets in `[0, frame_size)` vs a centred range.
20. An engine player concept with a `POSSESS` spawn tag vs a game-side binding.
21. dt = base period x scale (S1.15) vs more ticks per second (P1.7a).
22. Commands in their stamped step even when zero-time vs applying effects in zero-time steps.
23. Refusing foreign plugin sets and schemas vs D16.5's opaque carry, for the MVP period.
24. v8g's internal epoch vs `StepId` as page version.
25. Compare only on explicit compare-and-publish vs reverting no-op copies at seal.
26. 64-item leaves (checked by `v8g-6`) vs 1024-item pages.
27. Shared class rings, no preemption vs stealing deques or a reserved presentation worker.
28. `wait()` on a worker is a bug vs help-while-waiting.
29. The I/O and pipe-GC threads kept as P3.3 exceptions.
30. Non-`sim_input` settings restart-required except presentation's `live` fields.
31. Asset override by UID vs by path.
32. Raw shaders and fonts with vpath-derived UIDs vs authored meta UIDs and a shader cooker.
33. Hot reload through a build step vs development source mounts.
34. Strict lowercase ASCII vpaths, mod authors included.
35. One shaped text face now vs the baked ASCII atlas until localization.
36. Interpolation only (about 1.25 ticks of positional lag) until D10.1a.
37. Neutral declarations now, neutral recording, bindless and BDA removal right after the MVP.
38. No windowed game from the cut (M3) to parity (M8); the old demo at `legacy-demo`.
39. A fixed-point base-game controller vs a float controller and a tolerant avatar.
40. No residency drops; memory grows along the flight path.
41. `BlockDef` sim-consumed whole vs separate sim and presentation parts.
42. Generator strictness by `-ffp-contract=off` and goldens vs a fixed-point rewrite.

Left open (not needed by the MVP): the position of the menu-written settings layer relative to
`user` (D7.8 against P7.2); what the P7.2 asset "template" layer is (type-level or per-asset
parent); subtree deactivation (D3.2c); solver instances (?14.7); which outboxes a client receives
(D6.4).
