add_library(convertUTF8 STATIC simpleini/ConvertUTF.c)
target_compile_options(convertUTF8 PRIVATE "-Wno-error")
target_include_directories(convertUTF8 INTERFACE .)

add_library(SimpleIni INTERFACE)
target_include_directories(SimpleIni INTERFACE . )

target_link_libraries(SimpleIni INTERFACE convertUTF8)
