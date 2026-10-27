# libiberty demangler (c++filt, __cxa_demangle) fails on some Clang manglings: all already reported

**No new report.** The three failure kinds map onto existing GCC bugzilla entries (component
`demangler`):

| Kind | Example (from the testcase below) | Existing PR |
|---|---|---|
| `Tk` constrained template parameter in template args | `_Z1fITk1CiEvT_` | [119025](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=119025) (UNCONFIRMED; GCC itself emits `Tk` too) |
| `Tn <type>` non-type template parameter declaration in template args | `_Z1hIiTnN9enable_ifIXgtstT_Li1EEiE4typeELi0EEvS1_` | [124295](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124295) (NEW; Android NDK symbols, open question there: which compiler) |
| clone suffix after a **data** name (`.llvm.<n>`, also GCC's `.lto_priv.<n>`) | `_ZN12_GLOBAL__N_17counterE.llvm.7311954789195102804` | [59518](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=59518) (NEW, lists `_ZL1n.lto_priv.7`) |

Related, not the same: PR 100437 (hex digits in clone suffixes), PR 115245 (`Q` constraint
manglings).

Verified locally (2026-10-25): binutils `c++filt` 2.46 (Ubuntu), `abi::__cxa_demangle` from GCC
15.2 libstdc++ (`-static-libstdc++`) and from the system `libstdc++.so.6` 16-20260322 all fail on
the 6 symbols marked "fail" below; `llvm-cxxfilt` 21 demangles all of them. Trunk
`libiberty/cp-demangle.c` (checked 2026-10-25, last change d9d9119bf2 2026-07-01) parses
`Ty`/`Tn`/`Tt`/`Tp` only inside lambda template heads (`d_template_parm` via `d_template_head`),
not in `d_template_arg`, and has no `Tk` case. For data names, `d_encoding` sees the `.` after the
name, treats it as the start of a `<bare-function-type>` and fails, so `cplus_demangle_mangled_name`
never reaches the clone-suffix loop.

Files: `04-gcc-demangle/tk.cpp`, `a.cpp`, `b.cpp`, `dm.cpp` (prints `__cxa_demangle` results for
its arguments).

## Testcase

`tk.cpp` (clang++ 21: `clang++ -std=c++20 -c tk.cpp`):

```c++
template <class T> concept C = sizeof(T) > 1;

template <C T> void f(T) {}
template void f<int>(int);

template <class... A> struct S {
	template <C F> S(F &&) {}
};
void use() { S<int> s{0}; }

template <class T, int (*)[sizeof(T)] = nullptr> void g(T) {}
template void g<int>(int);

template <bool B, class T = void> struct enable_if {};
template <class T> struct enable_if<true, T> { using type = T; };
template <class T, typename enable_if<(sizeof(T) > 1), int>::type = 0> void h(T) {}
template void h<int>(int);
```

`a.cpp` / `b.cpp` (ThinLTO promotion of internal symbols):

```c++
// a.cpp
namespace { int counter; }
[[gnu::noinline]] static int bump() { return ++counter; }
int get() { return bump() + counter; }
// b.cpp
int get();
int main() { return get(); }
```

`clang++ -O2 -flto=thin -c a.cpp; clang++ -O2 -flto=thin -c b.cpp; clang++ -O2 -flto=thin -fuse-ld=lld a.o b.o -o lt; nm lt | grep '\.llvm\.'`

| Symbol | c++filt 2.46 / __cxa_demangle | llvm-cxxfilt 21 |
|---|---|---|
| `_Z1fITk1CiEvT_` | fail (-2) | `void f<int>(int)` |
| `_ZN1SIJiEEC2ITk1CiEEOT_` | fail (-2) | `S<int>::S<int>(int&&)` |
| `_Z1gIiTnPAstT__iLPA4_i0EEvS0_` | fail (-2) | `void g<int, (int (*) [4])0>(int)` |
| `_Z1hIiTnN9enable_ifIXgtstT_Li1EEiE4typeELi0EEvS1_` | fail (-2) | `void h<int, 0>(int)` |
| `_ZL4bumpv.llvm.7311954789195102804` | OK: `bump() [clone .llvm.7311954789195102804]` | OK |
| `_ZN12_GLOBAL__N_17counterE.llvm.7311954789195102804` | fail (-2) | `(anonymous namespace)::counter (.llvm.7311954789195102804)` |

GCC 15.2 with `-flto -flto-partition=max` on the same `a.cpp`/`b.cpp` produces
`_ZN12_GLOBAL__N_1L7counterE.lto_priv.0`, which c++filt also leaves alone (while
`_ZL4bumpv.lto_priv.0` works).

---

## Draft comment for PR 124295 (answers "what were they mangled with?")

> These are Clang manglings: since Clang 18 (`-fclang-abi-compat=17` restores the old form,
> e.g. `_Z1hIiLi0EEvT_`), a template argument list may contain `<template-param-decl> <template-arg>`, i.e. `Tn <type>` before a
> non-type argument whose parameter type is instantiation-dependent (and `Tk <name>` for a
> constrained type parameter, see PR 119025). Minimal source, clang++ 21 `-std=c++20`:
>
> ```c++
> template <bool B, class T = void> struct enable_if {};
> template <class T> struct enable_if<true, T> { using type = T; };
> template <class T, typename enable_if<(sizeof(T) > 1), int>::type = 0> void h(T) {}
> template void h<int>(int);
>
> template <class T, int (*)[sizeof(T)] = nullptr> void g(T) {}
> template void g<int>(int);
> ```
>
> gives `_Z1hIiTnN9enable_ifIXgtstT_Li1EEiE4typeELi0EEvS1_` and `_Z1gIiTnPAstT__iLPA4_i0EEvS0_`.
> c++filt 2.46 and `__cxa_demangle` (GCC 15.2, 16-20260322) fail on both; llvm-cxxfilt 21 gives
> `void h<int, 0>(int)` and `void g<int, (int (*) [4])0>(int)`. In trunk cp-demangle.c,
> `d_template_parm` (which knows `Tn`) is only called from `d_template_head` for lambdas, never from
> `d_template_arg`.

## Draft comment for PR 119025

> Clang emits the same `Tk` form (clang++ 21, `-std=c++20`):
> `template <class T> concept C = sizeof(T) > 1; template <C T> void f(T) {} template void f<int>(int);`
> gives `_Z1fITk1CiEvT_` (same string GCC 15 emits as the alias next to `_Z1fIiEvT_`), and a
> constrained constructor template `template <class... A> struct S { template <C F> S(F &&) {} };`
> gives `_ZN1SIJiEEC2ITk1CiEEOT_`. Both fail with c++filt 2.46 and `__cxa_demangle` from GCC 15.2
> and 16-20260322; llvm-cxxfilt 21 prints `void f<int>(int)` and `S<int>::S<int>(int&&)`.
> Probably best fixed together with PR 124295 (`Tn` in template args): both are
> `<template-arg> ::= <template-param-decl> <template-arg>`.

## Draft comment for PR 59518

> Same failure with Clang ThinLTO's promoted internal variables, e.g.
> `_ZN12_GLOBAL__N_17counterE.llvm.7311954789195102804` (functions like
> `_ZL4bumpv.llvm.7311954789195102804` demangle fine), and still with GCC 15.2's own
> `_ZN12_GLOBAL__N_1L7counterE.lto_priv.0` (`-flto -flto-partition=max`). Any clone suffix after a
> data name fails, e.g. `_ZN1a1bE.part.0`. Cause: `d_encoding` treats the `.` after a data
> `<name>` as the start of a `<bare-function-type>`, returns NULL, and the clone-suffix loop in
> `cplus_demangle_mangled_name` is never reached. Stopping at `.` in `d_encoding` (as for `\0` and
> `E`) when at top level would probably fix it (untested). llvm-cxxfilt 21 prints
> `(anonymous namespace)::counter (.llvm.7311954789195102804)`.
