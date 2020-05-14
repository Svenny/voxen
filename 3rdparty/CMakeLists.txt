# This cmake file are managing embedded 3party Voxen dependencies

include(ExternalProject)

## fmt dependency

# calculate coresponding library name
string(TOLOWER ${CMAKE_BUILD_TYPE} cmake_build_type_lower)
if(cmake_build_type_lower MATCHES debug)
    #DEBUG_POSTFIX d inside fmt
    set(fmt_library_filename libfmtd${CMAKE_STATIC_LIBRARY_SUFFIX})
else()
    set(fmt_library_filename libfmt${CMAKE_STATIC_LIBRARY_SUFFIX})
endif()

# create external project
set(fmt_builddir ${CMAKE_CURRENT_BINARY_DIR}/fmt_project_builddir)
ExternalProject_Add(
    fmt_project
    URL ${CMAKE_CURRENT_SOURCE_DIR}/fmt
    CMAKE_ARGS 
        -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
        -DCMAKE_C_COMPILER:STRING=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER:STRING=${CMAKE_CXX_COMPILER}
        -DFMT_DOC:STRING=OFF
        -DFMT_TEST:STRING=OFF
        -DFMT_FUZZ:STRING=OFF
        -DFMT_CUDA_TEST:STRING=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=true
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}
    BINARY_DIR ${fmt_builddir}
    INSTALL_COMMAND ""
    STEP_TARGETS build
    EXCLUDE_FROM_ALL TRUE
    BUILD_BYPRODUCTS ${fmt_builddir}/${fmt_library_filename}
)

ExternalProject_Get_Property(fmt_project binary_dir)

add_library(fmt::fmt STATIC IMPORTED GLOBAL)
set_target_properties(fmt::fmt PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/fmt/include
    IMPORTED_LOCATION ${binary_dir}/${fmt_library_filename}
    POSITION_INDEPENDENT_CODE ON
)
add_dependencies(fmt::fmt fmt_project-build)

## PlatformFolders dependency
add_subdirectory(PlatformFolders)

## cxxopts dependency
add_subdirectory(cxxopts)
