# `<stacktrace>`: `source_line()` / `source_file()` report the line after the call (return address not adjusted)

## Summary

`std::stacktrace_entry::source_line()` gives the line of the statement *after* the call for
caller frames, e.g. `g()` calling `f()` on line 13 is reported as line 14. `stl/src/stacktrace.cpp`
passes the captured address (a return address from `CaptureStackBackTrace`) straight to
`IDebugSymbols::GetLineByOffset`; the return address belongs to the instruction after the call,
which can be on a different line. `source_file()` and `to_string()` use the same lookup. The usual fix in
unwinders/symbolizers is to look up `address - 1` for return addresses; libstdc++'s
`std::stacktrace` (libbacktrace) reports the call line for caller frames (checked with GCC 15.2 on
Linux with a similar program).

## Environment

Windows 11 x64, VS 2026 Build Tools, MSVC 14.51.36231 (cl and STL), also clang-cl 21.1.8 with the
same STL. Current `main` of microsoft/STL has the same code (`source_line()` in
`stl/src/stacktrace.cpp`, last change 8cc5158ce4).

## Test (`09-msstl-stacktrace-line/st.cpp`)

```c++
#include <cstdio>
#include <stacktrace>

__declspec(noinline) void f()
{
	auto st = std::stacktrace::current(); // line 6
	for (const auto &e : st)
		std::printf("%u %s\n", e.source_line(), e.description().c_str());
}

__declspec(noinline) void g()
{
	f(); // line 13
	std::puts("g done"); // line 14
}

int main()
{
	g(); // line 19
	std::puts("main done"); // line 20
}
```

```
cl /nologo /std:c++latest /EHsc /Zi /Od st.cpp /link /DEBUG
st.exe
```

## Expected

```
6 st!f+0x30
13 st!g+0x9
19 st!main+0x9
...
```

## Actual

```
6 st_cl!f+0x30
14 st_cl!g+0x9
20 st_cl!main+0x9
288 st_cl!__scrt_common_main_seh+0x10F
```

Same with `/O2`. With clang-cl (`/Od`) even the first frame is off (`7 st_clang!f+0x2E`: the
instruction after the call to `current()` belongs to the `for` on line 7), then 14 and 20.

## Verified

On Windows 11 with MSVC 14.51.36231 (`/Od` and `/O2`) and clang-cl 21.1.8 (`/Od`), 2026-10-25.

No existing issue found in microsoft/STL (searched `<stacktrace>` issues for source_line,
line number, return address).
