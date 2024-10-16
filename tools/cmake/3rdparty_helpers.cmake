include_guard(GLOBAL)

# Link 3rdparty module against this dummy to suppress
# warnings and avoid `-Werror`-related build failures.
# We could simply disable `-Werror` but then build log will still be quite dirty.

add_library(dummy_suppress_3rdparty_warnings INTERFACE)
target_compile_options(dummy_suppress_3rdparty_warnings INTERFACE
	-Wno-extra-semi
	-Wno-implicit-fallthrough
	-Wno-nonportable-system-include-path
	-Wno-sign-conversion
	-Wno-switch-default
	-Wno-zero-as-null-pointer-constant
)
