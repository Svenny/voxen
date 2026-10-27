cmake_minimum_required(VERSION 3.28)
project(compile_only_modules CXX)
set(CMAKE_CXX_STANDARD 20)

add_library(m STATIC)
target_sources(m PUBLIC FILE_SET CXX_MODULES FILES m.cppm)

add_executable(use use.cpp)
target_link_libraries(use PRIVATE $<COMPILE_ONLY:m>)
