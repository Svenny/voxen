voxen_add_3rdparty_library(glfw)

# Platform-independent part, including the always-present null platform
target_sources(glfw PRIVATE
	glfw/src/context.c
	glfw/src/egl_context.c
	glfw/src/init.c
	glfw/src/input.c
	glfw/src/monitor.c
	glfw/src/null_init.c
	glfw/src/null_joystick.c
	glfw/src/null_monitor.c
	glfw/src/null_window.c
	glfw/src/osmesa_context.c
	glfw/src/platform.c
	glfw/src/vulkan.c
	glfw/src/window.c
)
target_include_directories(glfw SYSTEM PUBLIC glfw/include)
target_include_directories(glfw PRIVATE glfw/src)

if(WIN32)
	target_compile_definitions(glfw PRIVATE _GLFW_WIN32 UNICODE _UNICODE)
	target_sources(glfw PRIVATE
		glfw/src/wgl_context.c
		glfw/src/win32_init.c
		glfw/src/win32_joystick.c
		glfw/src/win32_module.c
		glfw/src/win32_monitor.c
		glfw/src/win32_thread.c
		glfw/src/win32_time.c
		glfw/src/win32_window.c
	)
elseif(LINUX)
	# Both X11 and Wayland backends, selected at runtime. GLFW loads the X11,
	# Wayland and xkbcommon libraries at runtime too, only their headers are
	# needed at build time (`xorg-dev`, `libwayland-dev`, `libxkbcommon-dev`).
	target_compile_definitions(glfw PRIVATE _GLFW_X11 _GLFW_WAYLAND _DEFAULT_SOURCE HAVE_MEMFD_CREATE)
	target_sources(glfw PRIVATE
		glfw/src/glx_context.c
		glfw/src/linux_joystick.c
		glfw/src/posix_module.c
		glfw/src/posix_poll.c
		glfw/src/posix_thread.c
		glfw/src/posix_time.c
		glfw/src/wl_init.c
		glfw/src/wl_monitor.c
		glfw/src/wl_window.c
		glfw/src/x11_init.c
		glfw/src/x11_monitor.c
		glfw/src/x11_window.c
		glfw/src/xkb_unicode.c
	)

	find_package(Threads REQUIRED)
	target_link_libraries(glfw PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)

	# Wayland protocol bindings are generated from the XML files GLFW ships
	find_program(VOXEN_WAYLAND_SCANNER wayland-scanner REQUIRED)
	set(GLFW_PROTOCOLS_DIR ${CMAKE_CURRENT_BINARY_DIR}/glfw-generated)
	file(MAKE_DIRECTORY ${GLFW_PROTOCOLS_DIR})
	target_include_directories(glfw PRIVATE ${GLFW_PROTOCOLS_DIR})

	foreach(protocol
		fractional-scale-v1
		idle-inhibit-unstable-v1
		pointer-constraints-unstable-v1
		relative-pointer-unstable-v1
		viewporter
		wayland
		xdg-activation-v1
		xdg-decoration-unstable-v1
		xdg-shell
	)
		set(xml ${CMAKE_CURRENT_SOURCE_DIR}/glfw/deps/wayland/${protocol}.xml)
		set(header ${GLFW_PROTOCOLS_DIR}/${protocol}-client-protocol.h)
		set(code ${GLFW_PROTOCOLS_DIR}/${protocol}-client-protocol-code.h)
		add_custom_command(OUTPUT ${header}
			COMMAND ${VOXEN_WAYLAND_SCANNER} client-header ${xml} ${header}
			DEPENDS ${xml} VERBATIM
		)
		add_custom_command(OUTPUT ${code}
			COMMAND ${VOXEN_WAYLAND_SCANNER} private-code ${xml} ${code}
			DEPENDS ${xml} VERBATIM
		)
		target_sources(glfw PRIVATE ${header} ${code})
	endforeach()
else()
	message(FATAL_ERROR "Unknown build platform")
endif()
