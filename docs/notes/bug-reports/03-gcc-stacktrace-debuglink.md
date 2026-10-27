# Additional data for GCC PR 123265 (std::stacktrace and separate debug files)

**Not a new report.** https://gcc.gnu.org/bugzilla/show_bug.cgi?id=123265 ("std::stacktrace reports
broken stacktraces when using separate debug symbols", NEW, filed 2025-12-22 against 14.2) already
describes this, with the same diagnosis (no `HAVE_LSTAT`/`HAVE_READLINK` in `BACKTRACE_CPPFLAGS`).
Draft comment below adds: still present in 15.2, the 16 snapshot and trunk configure; strace of the
lookup paths; a suggested configure change.

---

Draft comment:

> Still reproduces with GCC 15.2.0 (Ubuntu 15.2.0-16ubuntu1, x86_64-linux-gnu); I saw the same
> result earlier with a GCC 16 snapshot (16-20260322) as well. Trunk `libstdc++-v3/acinclude.m4`
> `GLIBCXX_ENABLE_BACKTRACE` still only adds `HAVE_ATOMIC_FUNCTIONS`, `HAVE_SYNC_FUNCTIONS`,
> `HAVE_DL_ITERATE_PHDR`, `HAVE_FCNTL`, `HAVE_GETEXECNAME` (and `BACKTRACE_ELF_SIZE`) to
> `BACKTRACE_CPPFLAGS`, so `elf.c` uses its dummy `lstat`/`readlink`. `nm` on 15.2's
> `libstdc++exp.a` confirms: none of the `std_stacktrace-*.o` objects reference `lstat` or
> `readlink`.
>
> Testcase:
>
> ```c++
> #include <cstdio>
> #include <stacktrace>
>
> [[gnu::noinline]] void f()
> {
> 	for (const auto &e : std::stacktrace::current())
> 		std::printf("%s at %s:%u\n", e.description().c_str(), e.source_file().c_str(), e.source_line());
> }
>
> int main()
> {
> 	f();
> }
> ```
>
> ```
> $ g++ -std=c++23 -g -O0 st.cpp -o st -lstdc++exp
> $ ./st
> f() at /tmp/t/st.cpp:6
> main at /tmp/t/st.cpp:12
> ...
> $ objcopy --only-keep-debug st st.debug
> $ objcopy --strip-debug --add-gnu-debuglink=st.debug st
> $ ./st
> f() at :0
> main at :0
> __libc_start_call_main at ../sysdeps/nptl/libc_start_call_main.h:59
> ...
> ```
>
> (Running it by absolute path makes no difference; gdb finds `st.debug` fine.) Since libstdc++
> passes no file name to `backtrace_create_state`, libbacktrace opens `/proc/self/exe`, and with
> the symlink not resolved it looks for the debuglink file next to `/proc/self/`. strace:
>
> ```
> openat(AT_FDCWD, "/proc/self/exe", O_RDONLY|O_CLOEXEC) = 3
> openat(AT_FDCWD, "/usr/lib/debug/.build-id/66/b104b2d511a1b0e2e2773338f1945282802ee8.debug", O_RDONLY|O_CLOEXEC) = -1 ENOENT
> openat(AT_FDCWD, "/proc/self/st.debug", O_RDONLY|O_CLOEXEC) = -1 ENOENT
> openat(AT_FDCWD, "/proc/self/.debug/st.debug", O_RDONLY|O_CLOEXEC) = -1 ENOENT
> openat(AT_FDCWD, "/usr/lib/debug//proc/self/st.debug", O_RDONLY|O_CLOEXEC) = -1 ENOENT
> ```
>
> Shared libraries are not affected because their paths come from `dl_iterate_phdr` and are real
> paths. The build-id lookup in `/usr/lib/debug/.build-id/` would work, but that directory is
> usually not writable by the user building the program.
>
> libbacktrace's own `configure.ac` does `AC_CHECK_FUNCS(lstat readlink)`. Something along these
> lines in `GLIBCXX_ENABLE_BACKTRACE` (following the existing `fcntl` check; untested) should fix it:
>
> ```
>   # Check for lstat and readlink, used to resolve /proc/self/exe when
>   # looking for a .gnu_debuglink file.
>   if test -n "${with_target_subdir}"; then
>      case "${host}" in
>      *-*-mingw*) have_lstat=no; have_readlink=no ;;
>      *) have_lstat=yes; have_readlink=yes ;;
>      esac
>   else
>     AC_CHECK_FUNC(lstat, [have_lstat=yes], [have_lstat=no])
>     AC_CHECK_FUNC(readlink, [have_readlink=yes], [have_readlink=no])
>   fi
>   if test "$have_lstat" = "yes"; then
>     BACKTRACE_CPPFLAGS="$BACKTRACE_CPPFLAGS -DHAVE_LSTAT=1"
>   fi
>   if test "$have_readlink" = "yes"; then
>     BACKTRACE_CPPFLAGS="$BACKTRACE_CPPFLAGS -DHAVE_READLINK=1"
>   fi
> ```

---

Verified locally (2026-10-25): the testcase and strace output above with GCC 15.2.0 on Ubuntu
x86_64 (binutils objcopy). The GCC 16 snapshot result is from an earlier run on the same machine;
that compiler has since been removed and was not re-run. The configure change was not built or
tested. Testcase file: `03-gcc-stacktrace-debuglink/st.cpp`.
