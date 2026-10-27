cmake_minimum_required(VERSION 3.28)
project(tp_unused CXX)
set(CMAKE_CXX_STANDARD 20)

add_library(m STATIC)
target_sources(m PUBLIC FILE_SET CXX_MODULES FILES m.cppm)
target_compile_options(m PRIVATE -Werror)
