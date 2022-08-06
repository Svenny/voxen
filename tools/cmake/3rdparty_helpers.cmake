include_guard(GLOBAL)

# Link 3rdparty module against this dummy to avoid "-Werror,-Wsign-conversion" build failures
add_library(dummy_wno_sign_conversion INTERFACE)
target_compile_options(dummy_wno_sign_conversion INTERFACE -Wno-sign-conversion)
