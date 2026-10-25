voxen_add_3rdparty_library(sdl)

# Video (windows, events, keyboard/mouse/pen/touch input, Vulkan surfaces), joystick
# and gamepad, haptic, sensor. No audio, 2D renderer, GPU API, camera, dialogs, tray,
# power, storage, process, locale/URL or `SDL_main`; no OpenGL/EGL and no dummy or
# offscreen video drivers. Configuration lives in `sdl-config/SDL_build_config.h`,
# keep this source list in sync with it.
target_sources(sdl PRIVATE
	sdl/src/SDL.c
	sdl/src/SDL_assert.c
	sdl/src/SDL_error.c
	sdl/src/SDL_guid.c
	sdl/src/SDL_hashtable.c
	sdl/src/SDL_hints.c
	sdl/src/SDL_list.c
	sdl/src/SDL_log.c
	sdl/src/SDL_properties.c
	sdl/src/SDL_utils.c
	sdl/src/atomic/SDL_atomic.c
	sdl/src/atomic/SDL_spinlock.c
	sdl/src/cpuinfo/SDL_cpuinfo.c
	sdl/src/events/SDL_categories.c
	sdl/src/events/SDL_clipboardevents.c
	sdl/src/events/SDL_displayevents.c
	sdl/src/events/SDL_dropevents.c
	sdl/src/events/SDL_events.c
	sdl/src/events/SDL_eventwatch.c
	sdl/src/events/SDL_keyboard.c
	sdl/src/events/SDL_keymap.c
	sdl/src/events/SDL_mouse.c
	sdl/src/events/SDL_pen.c
	sdl/src/events/SDL_quit.c
	sdl/src/events/SDL_touch.c
	sdl/src/events/SDL_windowevents.c
	sdl/src/filesystem/SDL_filesystem.c
	sdl/src/haptic/SDL_haptic.c
	sdl/src/haptic/hidapi/SDL_hidapihaptic.c
	sdl/src/haptic/hidapi/SDL_hidapihaptic_lg4ff.c
	sdl/src/hidapi/SDL_hidapi.c
	sdl/src/io/SDL_asyncio.c
	sdl/src/io/SDL_iostream.c
	sdl/src/io/generic/SDL_asyncio_generic.c
	sdl/src/joystick/SDL_gamepad.c
	sdl/src/joystick/SDL_joystick.c
	sdl/src/joystick/SDL_steam_virtual_gamepad.c
	sdl/src/joystick/controller_type.c
	sdl/src/joystick/hidapi/SDL_hidapi_8bitdo.c
	sdl/src/joystick/hidapi/SDL_hidapi_combined.c
	sdl/src/joystick/hidapi/SDL_hidapi_flydigi.c
	sdl/src/joystick/hidapi/SDL_hidapi_gamecube.c
	sdl/src/joystick/hidapi/SDL_hidapi_gip.c
	sdl/src/joystick/hidapi/SDL_hidapi_lg4ff.c
	sdl/src/joystick/hidapi/SDL_hidapi_luna.c
	sdl/src/joystick/hidapi/SDL_hidapi_ps3.c
	sdl/src/joystick/hidapi/SDL_hidapi_ps4.c
	sdl/src/joystick/hidapi/SDL_hidapi_ps5.c
	sdl/src/joystick/hidapi/SDL_hidapi_rumble.c
	sdl/src/joystick/hidapi/SDL_hidapi_shield.c
	sdl/src/joystick/hidapi/SDL_hidapi_sinput.c
	sdl/src/joystick/hidapi/SDL_hidapi_stadia.c
	sdl/src/joystick/hidapi/SDL_hidapi_steam.c
	sdl/src/joystick/hidapi/SDL_hidapi_steam_hori.c
	sdl/src/joystick/hidapi/SDL_hidapi_steam_triton.c
	sdl/src/joystick/hidapi/SDL_hidapi_steamdeck.c
	sdl/src/joystick/hidapi/SDL_hidapi_switch.c
	sdl/src/joystick/hidapi/SDL_hidapi_wii.c
	sdl/src/joystick/hidapi/SDL_hidapi_xbox360.c
	sdl/src/joystick/hidapi/SDL_hidapi_xbox360w.c
	sdl/src/joystick/hidapi/SDL_hidapi_xboxone.c
	sdl/src/joystick/hidapi/SDL_hidapi_zuiki.c
	sdl/src/joystick/hidapi/SDL_hidapijoystick.c
	sdl/src/joystick/hidapi/SDL_report_descriptor.c
	sdl/src/locale/SDL_locale.c
	sdl/src/main/SDL_main_callbacks.c
	sdl/src/misc/SDL_url.c
	sdl/src/render/SDL_render.c
	sdl/src/render/SDL_yuv_sw.c
	sdl/src/sensor/SDL_sensor.c
	sdl/src/sensor/dummy/SDL_dummysensor.c
	sdl/src/stdlib/SDL_crc16.c
	sdl/src/stdlib/SDL_crc32.c
	sdl/src/stdlib/SDL_getenv.c
	sdl/src/stdlib/SDL_iconv.c
	sdl/src/stdlib/SDL_malloc.c
	sdl/src/stdlib/SDL_memcpy.c
	sdl/src/stdlib/SDL_memmove.c
	sdl/src/stdlib/SDL_memset.c
	sdl/src/stdlib/SDL_murmur3.c
	sdl/src/stdlib/SDL_qsort.c
	sdl/src/stdlib/SDL_random.c
	sdl/src/stdlib/SDL_stdlib.c
	sdl/src/stdlib/SDL_string.c
	sdl/src/stdlib/SDL_strtokr.c
	sdl/src/thread/SDL_thread.c
	sdl/src/tray/SDL_tray_utils.c
	sdl/src/tray/dummy/SDL_tray.c
	sdl/src/time/SDL_time.c
	sdl/src/timer/SDL_timer.c
	sdl/src/video/SDL_RLEaccel.c
	sdl/src/video/SDL_blit.c
	sdl/src/video/SDL_blit_0.c
	sdl/src/video/SDL_blit_1.c
	sdl/src/video/SDL_blit_A.c
	sdl/src/video/SDL_blit_N.c
	sdl/src/video/SDL_blit_auto.c
	sdl/src/video/SDL_blit_copy.c
	sdl/src/video/SDL_blit_slow.c
	sdl/src/video/SDL_bmp.c
	sdl/src/video/SDL_clipboard.c
	sdl/src/video/SDL_fillrect.c
	sdl/src/video/SDL_pixels.c
	sdl/src/video/SDL_rect.c
	sdl/src/video/SDL_rotate.c
	sdl/src/video/SDL_stb.c
	sdl/src/video/SDL_stretch.c
	sdl/src/video/SDL_surface.c
	sdl/src/video/SDL_video.c
	sdl/src/video/SDL_vulkan_utils.c
	sdl/src/video/SDL_yuv.c
	sdl/src/video/yuv2rgb/yuv_rgb_sse.c
	sdl/src/video/yuv2rgb/yuv_rgb_std.c
)

# Our config header goes first, it replaces `SDL_build_config.h` from SDL's `include/build_config`
target_include_directories(sdl PRIVATE sdl-config sdl/src)
target_include_directories(sdl SYSTEM PUBLIC sdl/include)
target_compile_definitions(sdl PUBLIC SDL_STATIC_LIB)
# Vulkan headers for the surface creation code, see `patches/sdl/vulkan-headers.patch`
target_link_libraries(sdl PRIVATE 3rdparty::vulkan-headers)

if(WIN32)
	target_sources(sdl PRIVATE
		sdl/src/core/windows/SDL_hid.c
		sdl/src/core/windows/SDL_windows.c
		sdl/src/core/windows/SDL_xinput.c
		sdl/src/filesystem/windows/SDL_sysfilesystem.c
		sdl/src/filesystem/windows/SDL_sysfsops.c
		sdl/src/haptic/windows/SDL_dinputhaptic.c
		sdl/src/haptic/windows/SDL_windowshaptic.c
		sdl/src/joystick/windows/SDL_dinputjoystick.c
		sdl/src/joystick/windows/SDL_rawinputjoystick.c
		sdl/src/joystick/windows/SDL_windows_gaming_input.c
		sdl/src/joystick/windows/SDL_windowsjoystick.c
		sdl/src/joystick/windows/SDL_xinputjoystick.c
		sdl/src/libm/s_modf.c
		sdl/src/loadso/windows/SDL_sysloadso.c
		sdl/src/locale/windows/SDL_syslocale.c
		sdl/src/misc/windows/SDL_sysurl.c
		sdl/src/sensor/windows/SDL_windowssensor.c
		sdl/src/thread/generic/SDL_syscond.c
		sdl/src/thread/generic/SDL_sysrwlock.c
		sdl/src/thread/windows/SDL_syscond_cv.c
		sdl/src/thread/windows/SDL_sysmutex.c
		sdl/src/thread/windows/SDL_sysrwlock_srw.c
		sdl/src/thread/windows/SDL_syssem.c
		sdl/src/thread/windows/SDL_systhread.c
		sdl/src/thread/windows/SDL_systls.c
		sdl/src/time/windows/SDL_systime.c
		sdl/src/timer/windows/SDL_systimer.c
		sdl/src/video/windows/SDL_windowsclipboard.c
		sdl/src/video/windows/SDL_windowsevents.c
		sdl/src/video/windows/SDL_windowsframebuffer.c
		# Only C stubs without GameInput, but the Win32 video driver needs them
		sdl/src/video/windows/SDL_windowsgameinput.cpp
		sdl/src/video/windows/SDL_windowskeyboard.c
		sdl/src/video/windows/SDL_windowsmessagebox.c
		sdl/src/video/windows/SDL_windowsmodes.c
		sdl/src/video/windows/SDL_windowsmouse.c
		sdl/src/video/windows/SDL_windowsrawinput.c
		sdl/src/video/windows/SDL_windowsshape.c
		sdl/src/video/windows/SDL_windowsvideo.c
		sdl/src/video/windows/SDL_windowsvulkan.c
		sdl/src/video/windows/SDL_windowswindow.c
	)
	target_compile_definitions(sdl PRIVATE UNICODE _UNICODE)
	target_link_libraries(sdl PRIVATE
		kernel32 user32 gdi32 winmm imm32 ole32 oleaut32 version uuid advapi32 setupapi shell32
	)
elseif(LINUX)
	# Wayland and X11 video drivers, selected at runtime (see `SDL_HINT_VIDEO_DRIVER` in
	# `os/window.cpp`). Like the other optional system libraries, SDL loads them at runtime,
	# only their headers are needed at build time (`libwayland-dev`, `libxkbcommon-dev`, `xorg-dev`).
	target_sources(sdl PRIVATE
		sdl/src/core/linux/SDL_evdev.c
		sdl/src/core/linux/SDL_evdev_capabilities.c
		sdl/src/core/linux/SDL_evdev_kbd.c
		sdl/src/core/linux/SDL_threadprio.c
		sdl/src/core/unix/SDL_appid.c
		sdl/src/core/unix/SDL_gtk.c
		sdl/src/core/unix/SDL_poll.c
		sdl/src/dialog/unix/SDL_zenitymessagebox.c
		sdl/src/events/SDL_keysym_to_keycode.c
		sdl/src/events/SDL_keysym_to_scancode.c
		sdl/src/events/SDL_scancode_tables.c
		sdl/src/events/imKStoUCS.c
		sdl/src/filesystem/posix/SDL_sysfsops.c
		sdl/src/filesystem/unix/SDL_sysfilesystem.c
		sdl/src/haptic/linux/SDL_syshaptic.c
		sdl/src/joystick/linux/SDL_sysjoystick.c
		sdl/src/loadso/dlopen/SDL_sysloadso.c
		sdl/src/locale/unix/SDL_syslocale.c
		sdl/src/misc/unix/SDL_sysurl.c
		sdl/src/process/SDL_process.c
		sdl/src/process/posix/SDL_posixprocess.c
		sdl/src/thread/pthread/SDL_syscond.c
		sdl/src/thread/pthread/SDL_sysmutex.c
		sdl/src/thread/pthread/SDL_sysrwlock.c
		sdl/src/thread/pthread/SDL_syssem.c
		sdl/src/thread/pthread/SDL_systhread.c
		sdl/src/thread/pthread/SDL_systls.c
		sdl/src/time/unix/SDL_systime.c
		sdl/src/timer/unix/SDL_systimer.c
		sdl/src/video/wayland/SDL_waylandclipboard.c
		sdl/src/video/wayland/SDL_waylandcolor.c
		sdl/src/video/wayland/SDL_waylanddatamanager.c
		sdl/src/video/wayland/SDL_waylanddyn.c
		sdl/src/video/wayland/SDL_waylandevents.c
		sdl/src/video/wayland/SDL_waylandkeyboard.c
		sdl/src/video/wayland/SDL_waylandmessagebox.c
		sdl/src/video/wayland/SDL_waylandmouse.c
		sdl/src/video/wayland/SDL_waylandshmbuffer.c
		sdl/src/video/wayland/SDL_waylandutil.c
		sdl/src/video/wayland/SDL_waylandvideo.c
		sdl/src/video/wayland/SDL_waylandvulkan.c
		sdl/src/video/wayland/SDL_waylandwindow.c
		sdl/src/video/x11/SDL_x11clipboard.c
		sdl/src/video/x11/SDL_x11dyn.c
		sdl/src/video/x11/SDL_x11events.c
		sdl/src/video/x11/SDL_x11framebuffer.c
		sdl/src/video/x11/SDL_x11keyboard.c
		sdl/src/video/x11/SDL_x11messagebox.c
		sdl/src/video/x11/SDL_x11modes.c
		sdl/src/video/x11/SDL_x11mouse.c
		sdl/src/video/x11/SDL_x11pen.c
		sdl/src/video/x11/SDL_x11settings.c
		sdl/src/video/x11/SDL_x11shape.c
		sdl/src/video/x11/SDL_x11toolkit.c
		sdl/src/video/x11/SDL_x11touch.c
		sdl/src/video/x11/SDL_x11video.c
		sdl/src/video/x11/SDL_x11vulkan.c
		sdl/src/video/x11/SDL_x11window.c
		sdl/src/video/x11/SDL_x11xfixes.c
		sdl/src/video/x11/SDL_x11xinput2.c
		sdl/src/video/x11/SDL_x11xsync.c
		sdl/src/video/x11/SDL_x11xtest.c
		sdl/src/video/x11/edid-parse.c
		sdl/src/video/x11/xsettings-client.c
	)
	target_compile_definitions(sdl PRIVATE _REENTRANT)
	# Wayland driver includes EGL headers even without OpenGL, take SDL's copies (as SDL's own build does)
	target_include_directories(sdl SYSTEM PRIVATE sdl/src/video/khronos)

	find_package(Threads REQUIRED)
	target_link_libraries(sdl PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)

	# Optional system libraries, also loaded at runtime. These are detected rather than
	# required to keep the build working without them, but a build for distribution
	# must have them all: CI installs `libdbus-1-dev`, `libdecor-0-dev` and `libudev-dev`.
	find_package(PkgConfig REQUIRED)
	# D-Bus: IME (IBus and Fcitx) on X11, system theme, RealtimeKit thread priorities
	pkg_check_modules(VOXEN_SDL_DBUS QUIET dbus-1)
	if(VOXEN_SDL_DBUS_FOUND)
		target_include_directories(sdl SYSTEM PRIVATE ${VOXEN_SDL_DBUS_INCLUDE_DIRS})
		target_compile_definitions(sdl PRIVATE VOXEN_SDL_HAVE_DBUS)
		target_sources(sdl PRIVATE
			sdl/src/core/linux/SDL_dbus.c
			sdl/src/core/linux/SDL_fcitx.c
			sdl/src/core/linux/SDL_ibus.c
			sdl/src/core/linux/SDL_ime.c
			sdl/src/core/linux/SDL_progressbar.c
			sdl/src/core/linux/SDL_system_theme.c
		)
	else()
		message(WARNING "D-Bus headers not found (libdbus-1-dev), SDL is built without IME support on X11")
	endif()
	# libdecor: client-side window decorations on Wayland compositors that don't draw them (GNOME)
	pkg_check_modules(VOXEN_SDL_LIBDECOR QUIET libdecor-0>=0.2)
	if(VOXEN_SDL_LIBDECOR_FOUND)
		target_include_directories(sdl SYSTEM PRIVATE ${VOXEN_SDL_LIBDECOR_INCLUDE_DIRS})
		target_compile_definitions(sdl PRIVATE VOXEN_SDL_HAVE_LIBDECOR)
	else()
		message(WARNING "libdecor headers not found (libdecor-0-dev), SDL is built without Wayland window decorations")
	endif()
	# udev: joystick hotplug, falls back to inotify on `/dev/input` without it
	find_path(VOXEN_SDL_LIBUDEV_INCLUDE_DIR libudev.h)
	if(VOXEN_SDL_LIBUDEV_INCLUDE_DIR)
		target_compile_definitions(sdl PRIVATE VOXEN_SDL_HAVE_LIBUDEV)
		target_sources(sdl PRIVATE sdl/src/core/linux/SDL_udev.c)
	else()
		message(WARNING "udev headers not found (libudev-dev), SDL is built without udev support")
	endif()

	# Wayland protocol bindings are generated from the XML files SDL ships
	find_program(VOXEN_WAYLAND_SCANNER wayland-scanner REQUIRED)
	set(SDL_PROTOCOLS_DIR ${CMAKE_CURRENT_BINARY_DIR}/sdl-generated)
	file(MAKE_DIRECTORY ${SDL_PROTOCOLS_DIR})
	target_include_directories(sdl SYSTEM BEFORE PRIVATE ${SDL_PROTOCOLS_DIR})

	file(GLOB protocol_xmls RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/sdl/wayland-protocols
		${CMAKE_CURRENT_SOURCE_DIR}/sdl/wayland-protocols/*.xml)
	foreach(xml_name IN LISTS protocol_xmls)
		string(REGEX REPLACE "\\.xml$" "" protocol ${xml_name})
		set(xml ${CMAKE_CURRENT_SOURCE_DIR}/sdl/wayland-protocols/${xml_name})
		set(header ${SDL_PROTOCOLS_DIR}/${protocol}-client-protocol.h)
		set(code ${SDL_PROTOCOLS_DIR}/${protocol}-protocol.c)
		add_custom_command(OUTPUT ${header}
			COMMAND ${VOXEN_WAYLAND_SCANNER} client-header ${xml} ${header}
			DEPENDS ${xml} VERBATIM
		)
		add_custom_command(OUTPUT ${code}
			COMMAND ${VOXEN_WAYLAND_SCANNER} private-code ${xml} ${code}
			DEPENDS ${xml} VERBATIM
		)
		target_sources(sdl PRIVATE ${header} ${code})
	endforeach()
else()
	message(FATAL_ERROR "Unknown build platform")
endif()
