set(JSONCPP_WITH_TESTS OFF)
set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF)
set(JSONCPP_WITH_EXAMPLE OFF)
set(BUILD_SHARED_LIBS ON)

add_subdirectory(jsoncpp)
add_library(3rdparty::jsoncpp ALIAS jsoncpp_lib)

voxen_fixup_3rdparty_target(jsoncpp_lib)
