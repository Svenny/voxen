// SDL build configuration for Voxen, replaces SDL's `include/build_config/SDL_build_config.h`.
// Hand-maintained, keep in sync with the source list in `3rdparty/sdl.cmake`.
//
// Derived from SDL's own configurations, trimmed to what we build: on Linux
// from the header SDL's CMake generates (with `SDL_{AUDIO,GPU,RENDER,CAMERA,DIALOG,
// TRAY,POWER}=OFF`, `SDL_{OPENGL,OPENGLES,KMSDRM,OFFSCREEN,DUMMYVIDEO}=OFF`) on
// Ubuntu 26.04, on Windows from `SDL_build_config_windows.h`. Feature checks
// (`HAVE_*`) assume glibc 2.39+ and the Windows 10 SDK, our baselines.
// When bumping SDL, diff these upstream files between versions and port changes.

#ifndef SDL_build_config_h_
#define SDL_build_config_h_

#include <SDL3/SDL_platform_defines.h>

// Subsystems we don't build
#define SDL_AUDIO_DISABLED 1
#define SDL_CAMERA_DISABLED 1
#define SDL_DIALOG_DISABLED 1
#define SDL_GPU_DISABLED 1
#define SDL_POWER_DISABLED 1
#define SDL_RENDER_DISABLED 1

// Video: Vulkan surfaces only, no OpenGL/EGL
#define SDL_VIDEO_VULKAN 1

// Joystick, haptic: HIDAPI drivers on top of the platform ones
#define SDL_JOYSTICK_HIDAPI 1

#define HAVE_LIBC 1
#define HAVE_GCC_ATOMICS 1
#define HAVE_FLOAT_H 1
#define HAVE_LIMITS_H 1
#define HAVE_MATH_H 1
#define HAVE_SIGNAL_H 1
#define HAVE_STDARG_H 1
#define HAVE_STDDEF_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_WCHAR_H 1

// C library functions available on both platforms
#define HAVE_MALLOC 1
#define HAVE_ABS 1
#define HAVE_MEMSET 1
#define HAVE_MEMCPY 1
#define HAVE_MEMMOVE 1
#define HAVE_MEMCMP 1
#define HAVE_STRLEN 1
#define HAVE_STRCHR 1
#define HAVE_STRRCHR 1
#define HAVE_STRSTR 1
#define HAVE_STRPBRK 1
#define HAVE_STRTOL 1
#define HAVE_STRTOUL 1
#define HAVE_STRTOLL 1
#define HAVE_STRTOULL 1
#define HAVE_STRTOD 1
#define HAVE_ATOI 1
#define HAVE_ATOF 1
#define HAVE_STRCMP 1
#define HAVE_STRNCMP 1
#define HAVE_VSSCANF 1
#define HAVE_VSNPRINTF 1
#define HAVE_ACOS 1
#define HAVE_ACOSF 1
#define HAVE_ASIN 1
#define HAVE_ASINF 1
#define HAVE_ATAN 1
#define HAVE_ATANF 1
#define HAVE_ATAN2 1
#define HAVE_ATAN2F 1
#define HAVE_CEIL 1
#define HAVE_CEILF 1
#define HAVE_COS 1
#define HAVE_COSF 1
#define HAVE_EXP 1
#define HAVE_EXPF 1
#define HAVE_FABS 1
#define HAVE_FABSF 1
#define HAVE_FLOOR 1
#define HAVE_FLOORF 1
#define HAVE_FMOD 1
#define HAVE_FMODF 1
#define HAVE_ISINF 1
#define HAVE_ISINF_FLOAT_MACRO 1
#define HAVE_ISNAN 1
#define HAVE_ISNAN_FLOAT_MACRO 1
#define HAVE_LOG 1
#define HAVE_LOGF 1
#define HAVE_LOG10 1
#define HAVE_LOG10F 1
#define HAVE_LROUND 1
#define HAVE_LROUNDF 1
#define HAVE_POW 1
#define HAVE_POWF 1
#define HAVE_ROUND 1
#define HAVE_ROUNDF 1
#define HAVE_SCALBN 1
#define HAVE_SCALBNF 1
#define HAVE_SIN 1
#define HAVE_SINF 1
#define HAVE_SQRT 1
#define HAVE_SQRTF 1
#define HAVE_TAN 1
#define HAVE_TANF 1
#define HAVE_TRUNC 1
#define HAVE_TRUNCF 1

#if defined(SDL_PLATFORM_WIN32)

#define HAVE_DDRAW_H 1
#define HAVE_DINPUT_H 1
#define HAVE_DXGI_H 1
#define HAVE_DXGI1_6_H 1
#define HAVE_XINPUT_H 1
#define HAVE_WINDOWS_GAMING_INPUT_H 1
#define HAVE_ROAPI_H 1
#define HAVE_SHELLSCALINGAPI_H 1
#define HAVE_TPCSHRD_H 1
#define HAVE_SENSORSAPI_H 1

#define HAVE__STRREV 1
#define HAVE__COPYSIGN 1
#define HAVE__FSEEKI64 1

// No GameInput, it needs the GDK (or a recent Windows SDK) and C++ sources
#define SDL_JOYSTICK_DINPUT 1
#define SDL_JOYSTICK_RAWINPUT 1
#define SDL_JOYSTICK_WGI 1
#define SDL_JOYSTICK_XINPUT 1
#define SDL_HAPTIC_DINPUT 1

#define SDL_SENSOR_WINDOWS 1
#define SDL_LOADSO_WINDOWS 1
#define SDL_THREAD_GENERIC_COND_SUFFIX 1
#define SDL_THREAD_GENERIC_RWLOCK_SUFFIX 1
#define SDL_THREAD_WINDOWS 1
#define SDL_TIME_WINDOWS 1
#define SDL_TIMER_WINDOWS 1
#define SDL_FILESYSTEM_WINDOWS 1
#define SDL_FSOPS_WINDOWS 1
#define SDL_TRAY_DUMMY 1

#define SDL_VIDEO_DRIVER_WINDOWS 1

#elif defined(SDL_PLATFORM_LINUX)

#define HAVE_ALLOCA_H 1
#define HAVE_ICONV_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_MALLOC_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STRINGS_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_LINUX_INPUT_H 1

#define HAVE_DLOPEN 1
#define HAVE_FDATASYNC 1
#define HAVE_GETENV 1
#define HAVE_GETHOSTNAME 1
#define HAVE_SETENV 1
#define HAVE_PUTENV 1
#define HAVE_UNSETENV 1
#define HAVE_BCOPY 1
#define HAVE_WCSLEN 1
#define HAVE_WCSNLEN 1
#define HAVE_WCSLCPY 1
#define HAVE_WCSLCAT 1
#define HAVE_WCSSTR 1
#define HAVE_WCSCMP 1
#define HAVE_WCSNCMP 1
#define HAVE_WCSTOL 1
#define HAVE_STRNLEN 1
#define HAVE_STRLCPY 1
#define HAVE_STRLCAT 1
#define HAVE_INDEX 1
#define HAVE_RINDEX 1
#define HAVE_STRTOK_R 1
#define HAVE_COPYSIGN 1
#define HAVE_COPYSIGNF 1
#define HAVE_ISINFF 1
#define HAVE_ISNANF 1
#define HAVE_MODF 1
#define HAVE_MODFF 1
#define HAVE_FOPEN64 1
#define HAVE_FSEEKO 1
#define HAVE_FSEEKO64 1
#define HAVE_MEMFD_CREATE 1
#define HAVE_POSIX_FALLOCATE 1
#define HAVE_SIGACTION 1
#define HAVE_SIGTIMEDWAIT 1
#define HAVE_SA_SIGACTION 1
#define HAVE_ST_MTIM 1
#define HAVE_SETJMP 1
#define HAVE_NANOSLEEP 1
#define HAVE_GMTIME_R 1
#define HAVE_LOCALTIME_R 1
#define HAVE_NL_LANGINFO 1
#define HAVE_SYSCONF 1
#define HAVE_CLOCK_GETTIME 1
#define HAVE_GETPAGESIZE 1
#define HAVE_ICONV 1
#define HAVE_PTHREAD_SETNAME_NP 1
#define HAVE_SEM_TIMEDWAIT 1
#define HAVE_GETAUXVAL 1
#define HAVE_PPOLL 1
#define HAVE__EXIT 1
#define HAVE_GETRESUID 1
#define HAVE_GETRESGID 1
#define HAVE_INOTIFY_INIT1 1
#define HAVE_INOTIFY 1
#define HAVE_O_CLOEXEC 1

#define SDL_INPUT_LINUXEV 1
#define SDL_INPUT_LINUXKD 1
#define SDL_JOYSTICK_LINUX 1
#define SDL_HAPTIC_LINUX 1
#define SDL_SENSOR_DUMMY 1
#define SDL_LOADSO_DLOPEN 1
#define SDL_THREAD_PTHREAD 1
#define SDL_THREAD_PTHREAD_RECURSIVE_MUTEX 1
#define SDL_TIME_UNIX 1
#define SDL_TIMER_UNIX 1
#define SDL_FILESYSTEM_UNIX 1
#define SDL_FSOPS_POSIX 1
#define SDL_PROCESS_POSIX 1
#define SDL_TRAY_DUMMY 1

// Optional system libraries, `3rdparty/sdl.cmake` detects their headers
#ifdef VOXEN_SDL_HAVE_DBUS
#define HAVE_DBUS_DBUS_H 1
#define HAVE_FCITX 1
// IBus is talked to over D-Bus too, see `patches/sdl/ibus-no-glib.patch`
#define HAVE_IBUS_IBUS_H 1
#define SDL_USE_IME 1
#endif
#ifdef VOXEN_SDL_HAVE_LIBUDEV
#define HAVE_LIBUDEV_H 1
#define SDL_UDEV_DYNAMIC "libudev.so.1"
#endif

// Both X11 and Wayland, all of their libraries are loaded at runtime
#define SDL_VIDEO_DRIVER_WAYLAND 1
#define SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC "libwayland-client.so.0"
#define SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC_CURSOR "libwayland-cursor.so.0"
#define SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC_EGL "libwayland-egl.so.1"
#define SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC_XKBCOMMON "libxkbcommon.so.0"
// Minimal versions SDL may assume symbols of at compile time, it checks for newer ones at runtime
#define SDL_XKBCOMMON_VERSION_MAJOR 1
#define SDL_XKBCOMMON_VERSION_MINOR 0
#define SDL_XKBCOMMON_VERSION_PATCH 0
#ifdef VOXEN_SDL_HAVE_LIBDECOR
#define HAVE_LIBDECOR_H 1
#define SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC_LIBDECOR "libdecor-0.so.0"
#define SDL_LIBDECOR_VERSION_MAJOR 0
#define SDL_LIBDECOR_VERSION_MINOR 2
#define SDL_LIBDECOR_VERSION_PATCH 0
#endif

#define SDL_VIDEO_DRIVER_X11 1
#define SDL_VIDEO_DRIVER_X11_DYNAMIC "libX11.so.6"
#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XCURSOR "libXcursor.so.1"
#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XEXT "libXext.so.6"
#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XFIXES "libXfixes.so.3"
#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XINPUT2 "libXi.so.6"
#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XRANDR "libXrandr.so.2"
#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XSS "libXss.so.1"
#define SDL_VIDEO_DRIVER_X11_HAS_XKBLIB 1
#define SDL_VIDEO_DRIVER_X11_SUPPORTS_GENERIC_EVENTS 1
#define SDL_VIDEO_DRIVER_X11_XCURSOR 1
#define SDL_VIDEO_DRIVER_X11_XFIXES 1
#define SDL_VIDEO_DRIVER_X11_XINPUT2 1
#define SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH 1
#define SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_SCROLLINFO 1
#define SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_GESTURE 1
#define SDL_VIDEO_DRIVER_X11_XRANDR 1
#define SDL_VIDEO_DRIVER_X11_XSCRNSAVER 1
#define SDL_VIDEO_DRIVER_X11_XSYNC 1

// Not x86 or not this kind of x86
#define SDL_DISABLE_LSX 1
#define SDL_DISABLE_LASX 1
#define SDL_DISABLE_NEON 1

#else
#error Unsupported platform
#endif

#endif // SDL_build_config_h_
