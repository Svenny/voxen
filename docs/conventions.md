# Conventions

How the code in `include/`, `src/`, `tests/` and `tools/` is written; module boundaries,
directories and their namespaces are in [architecture.md](architecture.md).

Formatting is whatever `.clang-format` produces (clang-format 21, checked in CI): tabs of width 3,
120 columns, `T *p` / `T &r`.

## Files

- snake_case names, `.hpp` and `.cpp`. `include/<module>/foo.hpp` pairs with `src/<module>/foo.cpp`.
  A large class may spread over more sources with a suffix: `vk_device.cpp`, `vk_device_wrappers.cpp`.
- A file is named after its main type, with the module prefix where the bare name is generic:
  `vk_device.hpp` has `vk::Device`, `land_chunk.hpp` has `land::Chunk`.
- Private (non-installed, implementation-only) headers live next to their sources in `src/` and end
  with `_priv.hpp`; `foo_priv.hpp` pairs with `foo.cpp`: `src/voxen/svc/messaging_priv.hpp`.
- `<module>_fwd.hpp` forward-declares an engine module's types, including the `detail::*Impl` pimpl
  targets (`land_fwd.hpp`, `svc_fwd.hpp`). Headers that only mention a type include it instead of
  the full definition. `m` has `m/fwd.hpp` in its own style.
- Module constants: `<module>_consts.hpp` in `include/`, `<module>_consts_priv.hpp` in `src/`, both
  in `namespace <module>::Consts`.
- `.in` files are X-macro lists (`include/voxen/gfx/vk/api_device.in`): no `#pragma once`, included
  between `#define VK_API_ENTRY(x) ...` and `#undef VK_API_ENTRY`.
- Every header starts with `#pragma once`, no include guards. Files are UTF-8 without BOM.

## Includes

- `<>` for anything on an include path, own module included: `#include <voxen/svc/engine.hpp>`.
  `""` only for private headers in the same `src/` directory, test helpers (`"../test_common.hpp"`)
  and `.in` files included from a header.
- Blocks separated by an empty line, each sorted by clang-format, in this order:

  ```cpp
  #include <voxen/svc/engine.hpp>      // own header: the .cpp's pair, or the tested header

  #include <voxen/log.hpp>             // other voxen/ headers
  #include <voxen/svc/config.hpp>

  #include "task_queue_set_priv.hpp"   // private headers, test helpers

  #include <extras/defer.hpp>          // extras/, m/, tk/: one block each, no settled order

  #include <tk/cli.hpp>

  #include <fmt/format.h>              // third-party

  #include <cstdint>                   // standard library
  ```
- Include what you use, standard headers too (`<cstdint>` for `uint32_t`). Deliberate re-exports
  are marked with `// IWYU pragma: export` (the test helpers).

## Naming

Engine style, used by `voxen`, `tk`, `tools` and `src/game`:

| What | Style | Example |
|---|---|---|
| Types, enums, aliases | PascalCase | `ServiceLocator`, `FileFlags` |
| Interfaces | `I` + PascalCase | `IService`, `IRenderPass` |
| Concepts | `C` + PascalCase | `CService`, `CMessageType` |
| Typed twin of a non-template | `T` + PascalCase | `TPipeMemoryAllocator<T>`, `TServiceFactoryFunction<S>` |
| Functions, methods | camelCase | `requestService()`, `alignUp()` |
| Variables, parameters, public fields | snake_case | `queue_family`, `tick_id` |
| Non-public members | `m_` + snake_case | `m_impl`, `m_handle` |
| Static data members and function statics | `s_` + snake_case | `Config::s_instance`, `s_last_warn_time` |
| Namespace-scope variables (anonymous namespace included) / `thread_local` | `g_` / `t_` + snake_case | `g_current_level`, `t_this_thread_slab` |
| Constants (`constexpr`, `static const`), locals too | SCREAMING_CASE | `SERVICE_UID`, `MAX_PENDING_FRAMES` |
| Template parameters | `T`, `F`, `N`... or PascalCase; values SCREAMING_CASE | `typename Allocator`, `bool FAIL_ONLY` |
| Macros | SCREAMING_CASE with a library prefix | `VOXEN_API`, `EXTRAS_NO_UNIQUE_ADDRESS` |
| Class-like namespaces | PascalCase | `os::Futex`, `debug::UidRegistry` |

- Accessors are nouns without `get`: `handle()`, `info()`, `level()`; setters are `setLevel()`.
  `get*` is for lookups and computed values (`getProcessId()`, `getVkResultString()`).
  Fallible twins of throwing functions are `try*`: `File::open()` / `File::tryOpen()`.
- STL protocol names stay snake_case where the language or std expects them: `try_lock()`,
  `lock_shared()`, `begin()`, ADL `make_error_condition()`, coroutine `promise_type`.
- Services are `<Name>Service` (`TaskService`, `world::ControlService`) unless a more precise word
  describes their role (`PipeMemoryAllocator`, `AsyncCounterTracker`).

`extras` follows the standard library: snake_case for everything (`dyn_array`, `enum_flags`,
`fnref_invocable`), `m_` members, SCREAMING_CASE macros prefixed `EXTRAS_`, and lowercase `defer`
and `defer_fail` as statement-like macros.

`m` follows HLSL: lowercase types and aliases (`vector<T, N>`, `float3`, `rgba8_srgb`), HLSL names
for HLSL functions (`saturate`, `frac`), GLSL names to fill gaps (`lessThan`), camelCase for its own
(`fromRotationMatrix`), `C`-prefixed concepts (`CScalar`).

### Enums

- `enum class` with PascalCase values by default: `VoxenErrc::FileNotFound`.
- A plain enum nested in a class, with values prefixed by the enum's exact name and a `...Count`
  sentinel, when values are array indices: `Device::QueueMain`, `Device::QueueCount`,
  `PipelineCollection::GraphicsPipelineLandChunkMesh`.
- Bit flags: `enum class FooBit` plus `using Foos = extras::enum_flags<FooBit>` (`FileFlagsBit`, `FileFlags`).

## Namespaces

- `namespace a::b` with the brace on its own line, an empty line inside, and the closing comment
  `} // namespace a::b`. No `using namespace` in headers, rarely in sources (literals, demos, `config.cpp`).
- `detail` holds implementation types that public headers must name (`svc::detail::TaskServiceImpl`)
  and types shared by several sources. Anything local to one source goes into an anonymous
  namespace, never file-scope `static`.
- `tk` puts each multi-name header into a lowercase sub-namespace named after it, like
  `std::filesystem`: `tk::fs`, `tk::json`, `tk::cli`. Single types stay in `tk` (`tk::UID`).
- Outside `tk`, free functions are grouped in a class-like namespace: a PascalCase namespace named
  after the header and called like a static class, never a class with only static members. It is
  declared in the combined form; state, if any, stays in an anonymous namespace of the source
  (`Log`, `svc::FileManager`), and helpers the header must expose go into a nested `detail`:

  ```cpp
  namespace voxen::gfx::vk::VulkanUtils
  {
  std::string_view getVkResultString(VkResult result) noexcept;
  } // namespace voxen::gfx::vk::VulkanUtils

  Log::warn("Vulkan API error: {}", VulkanUtils::getVkResultString(result));
  ```

## Classes

- `struct` for plain data with public fields and default member initializers (`uint32_t line = 0;`),
  `class` for anything with invariants.
- Declare all five special members explicitly, `= default` or `= delete`, in this order:
  `Foo(Foo &&)`, `Foo(const Foo &)`, `operator=(Foo &&)`, `operator=(const Foo &)`, `~Foo()`.
- `override` on overrides without repeating `virtual`; `final` on services.
- Pimpl is `extras::pimpl<detail::FooImpl, SIZE, ALIGN> m_impl;` with `FooImpl` forward-declared in `<module>_fwd.hpp` and defined in the source.

## Services and UIDs

- A service implements `svc::IService`, has `constexpr static UID SERVICE_UID = UID("...")` and
  `UID serviceUid() const noexcept override { return SERVICE_UID; }`. It is constructed by a factory
  registered with `ServiceLocator::registerServiceFactory<S>()`; the constructor takes
  `svc::ServiceLocator &svc` and, if configurable, a nested `struct Config` by value.
- Message payloads carry `constexpr static UID MESSAGE_UID` and `MESSAGE_CLASS`.
- Other fixed UIDs are `*_UID` constants (`LAND_SERVICE_SENDER_UID`). Generate them with
  `tools/uidgen`; the `UID("...")` constructor is `consteval` and rejects malformed strings.
  Give them a name via `debug::UidRegistry::registerLiteral(uid, "voxen::land::LandService")`.

## Errors

- The engine throws `voxen::Exception`, almost always through `Exception::fromError(VoxenErrc::...,
  "static details")` or `Exception::fromErrorCode(ec, ...)` for platform/library codes. Subclass only
  to carry extra data (`vk::VulkanException` keeps the `VkResult`). Details strings are short,
  lowercase and static; log the variable parts first:

  ```cpp
  Log::error("Option {}/{} (int32) not found", section, parameter_name);
  throw Exception::fromError(VoxenErrc::OptionMissing, "missing config option assumed existing", loc);
  ```
- Where failure is a normal outcome, return a value instead: `std::expected<T, std::error_condition>`
  next to the throwing form (`tryOpen()`), or `std::optional`/`bool` with the error logged.
- Functions that throw or log on behalf of their caller take
  `std::source_location loc = std::source_location::current()` as the last parameter (`where` in
  `Log` and `bugFound`).
- `tk` never throws to its callers (no engine dependency): it returns `std::expected<T, std::string>`
  or `std::expected<T, ParseError>`.
- `assert()` checks preconditions and invariants in debug builds (180+ uses).
  `debug::bugFound("message")` is for bugs that must stop release builds too: it logs a stacktrace
  and aborts.
- `static_assert` for layout and size assumptions.

## Other idioms

- `noexcept` on everything that cannot throw, including trivial accessors.
- `constexpr static`, in this order, for static constants (`m` and `tk` included).
- `[[likely]]`/`[[unlikely]]` after the condition: `if (index.stop_flag) [[unlikely]] {`.
  `[[nodiscard]]` only on allocation functions. `[[maybe_unused]]` for variables read only by `assert`.
  Unused parameters keep their name in a comment: `void onFrameTickEnd(FrameTickId /*current_tick*/)`.
- `constexpr` wherever possible (all of `m` but trigonometry, `tk/hash`), `consteval` for literal validation,
  `if !consteval` for runtime-only fast paths.
- `defer { ... };` and `defer_fail { ... };` from `extras/defer.hpp` for cleanup, especially
  rolling back in constructors: `createDevice(); defer_fail { vkDestroyDevice(...); };`.
- Ownership: `std::unique_ptr` (or a pool's `Ptr` alias) owns, references for required dependencies
  (`ServiceLocator &m_svc`), raw pointers don't own and may be null. Shared lifetime uses
  `std::shared_ptr` (messaging, async I/O results) or `extras::refcnt_ptr`.
- Locking: `os::FutexLock`/`os::FutexRWLock` with `std::lock_guard`, not `std::mutex` (2 exceptions:
  `extras::fixed_pool`, which can't use `os`, and `ServiceLocator`). There are no
  thread-safety annotations; document it in the comment, e.g. `// This class is NOT thread-safe.`
- Integer types unqualified (`uint32_t`, `size_t`, never `std::uint32_t`). `static_cast` or a
  functional cast for arithmetic (`uint64_t(x)`); no C-style casts besides `(void)`.
- `fmt`, not `std::format`. `m` and `tk` get it only through their opt-in `format.hpp`, `extras` never.

## Logging

- `Log::trace/debug/info/warn/error/fatal(fmt_string, args...)` with fmt `{}` placeholders and the
  caller's source location captured automatically. Levels are documented in `voxen/log.hpp`.
- Messages start with a capital letter and have no trailing period: `"Stopping task service"`.
- Only the engine logs; `tk` returns errors and tools print with `fprintf(stderr, ...)`.

## Exports

- `VOXEN_API` on public classes and free functions of libvoxen: `class VOXEN_API Engine`,
  `VOXEN_API void bugFound(...)`. Header-only types and templates don't need it.
- `VOXEN_LOCAL` hides private members of exported classes (`VOXEN_LOCAL void swap(Window &)`).

## Comments

- `//` only. API docs are full sentences on the lines above the declaration, code in backticks.
  No Doxygen. `/* */` only inline, e.g. for commented-out parameter names.
- Markers are `TODO:`, `NOTE:`, `WARNING:` and `XXX:` (for known caveats), always with the colon.

## Tests

- Catch2, one executable per top directory (`test-voxen`, `test-tk`...). Test code lives in the
  namespace under test (`namespace voxen::svc`), helpers in an anonymous namespace.
- `TEST_CASE("'ServiceLocator' circular dependency", "[voxen::svc::service_locator]")`: the name
  starts with the quoted type or header, the tag is the namespace plus the header stem (`util` is
  plain `voxen::`). Most `m` and `tk` tests name the subject unquoted (`"TOML parses tables"`).
- Each tag is registered in the directory's `CMakeLists.txt`:
  `add_test(NAME voxen-svc-config COMMAND test-voxen "[voxen::svc::config]")`.
- `CHECK` by default, `REQUIRE` when continuing makes no sense; `SECTION` for variations on one setup.
  Engine errors are matched with `CHECK_THROWS_MATCHES(expr, Exception, errcExceptionMatcher(VoxenErrc::X))`.
