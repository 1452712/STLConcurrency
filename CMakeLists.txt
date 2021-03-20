# Tips:
## cmake (cmake -P <script>.cmake; include(modules) from CMAKE_MODULE_PATH)
## target_precomiple_headers(Foo PUBLIC “pch.h”) and remove #include “pch.h”
## set(CMAKE_DEBUG_POSTFIX “-d”)
## remove FILE(Blob) , link_..., include_....
## always "${MY_PATH}" to aviod space contianed in a path
## avoid using environment var: set(ENV{var} val), $ENV{var}
## For package:
## find_package(Foo)
## install(TARGETS Foo EXPORT FooTargets
##         LIBRARY DESTINATION lib
##         ARCHIVE DESTINATION lib
##         RUNTIME DESTINATION bin
##         INCLUDES DESTINATION include
##         )
## install(EXPORT FooTargets
##         FILE FooTargets.cmake
##         NAMESPACE Foo::
##         DESTINATION lib/cmake/Foo
##         )

cmake_minimum_required(VERSION 3.7)
# support as low as 3.7 but have also tested it 
# with the new policy settings up to 3.19
#cmake_minimum_required(VERSION 3.7...3.19)

# to support non cmd windows build for older MSVC
if(${CMAKE_VERSION} VERSION_LESS 3.19)
    cmake_policy(VERSION ${CMAKE_MJOR_VERSION}.${CMAKE_MINOR_VERSION})
else()
    cmake_policy(3.19)
endif()

# Standard options
set(CMAKE_BUILD_TYPE "Release, RelWithDebInfo, Debug")
set(CMAKE_INSTALL_PREFIX "~/.local")
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/scripts" ${CMAKE_MODULE_PATH})
set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING OFF)

# Require out-of-source builds
file(TO_CMAKE_PATH "${CMAKE_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if (EXISTS "$LOC_PATH}")
    message(FATAL_ERROR "You cannot build in a source directory (or any directory with a CMakeLists.txt file). Please make a build subdirectory. Feel free to remove CMakeCache.txt and CMakeFiles.")
endif()

# global properties
# target properties will be initialized from a matching variable with CMAKE_XXXXX
set(CMAKE_CXX_STANDARD 20)

# access CMake vars from config files (.in)
#  this will copy .in and substitute all CMake variables it finds
configure_file( # COPY_ONLY: only set if not exist
    "${CMAKE_MODULE_PATH}/Version.h.in"
    "${CMAKE_BINARY_DIR}/Version.h"
)

# set a project
project(MyProj VERSION 0.1 # will asl set PROJECT_VERSION
               DESCRIPTION "test cmake"
               LANGUAGES CXX) # default: C CXX; options: C, CXX, Fortran, ASM, CUDA, CSharp, SWIFT


# alternative:
#   read in from source files (available with or without CMake). 
#   useful especaily for header only proj.
## asuum canonical version listed in a single line
## pick up several parts from MAJOR, MINOR, etc.
# set(VERSION_REGEX "#define MY_VERSION[ \t]+\"(.+)\"")
## picks lines that match a regex
# file(STRINGS "file_contains_version.hpp" VERSION_STRING REGEX ${VERSION_REGEX})
## pick out the parentheses capture group with the version part
## "\\1" output only that one group
# string(REGEX REPLACE ${VERSION_REGEX} "\\1" VERSION_STRING "${VERSION_STRING}")
## auto get PROJECT_VERSION_MAJOR etc.
# project(MyProj VERSION ${VERSION_STRING})

# declare a variable and set it if not set
# thus it can be override on cmd
set(MY_CACHE_VARIABLE "value" CACHE STRING "descirption")
# force setting
set(MY_GLOBAL_VAR "value" CACHE STRING "" FORCE)
# make it a make-shift global variable
mark_as_advanced(MY_GLOBAL_VAR)
# alt: set(MY_GLOBALE_VAR "value" CACHE INTERNAL STRING "" FORCE)

# shortcut for bool
option(MY_OPTION "settable from cmd" OFF) # or ON

set(MY_LIST "one" "two")
# set(MY_LIST "one;two")
# set(MY_LIST "one two") separate_arguments(MY_LIST)

# run a command at config
find_package(Git QUIET) # use ${CMAKE_COMMAND} / find_package(pkg) / find_program to avoid hard code
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    # update all git submodules
    execute_process(COMMAND "${GIT_EXECUTABLE}" submodule update --init --recursive
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                    RESULT_VARIABLE GIT_SUBMOD_RESULT) # RESULT_VARIABLE check return vode, OUTPUT_VARIABLE get output
    if(NOT GIT_SUBMOD_RESULT EQUAL "0")
        message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
    endif()
endif()

###################### Helper Functions ####################

function(PARSE_ARG)
    if(${CMAKE_VERSION} VERSION_LESS 3.5)
        include(CMakeParseArguments)
    endif()
    cmake_parse_arguments(
        MY_PREFIX
        "SINGLE;ANOTHER"
        "ONE_VALUE;ALSO_ONE_VALUE"
        "MULTI_VALUES"
        ${ARGN}
    )
endfunction()

function(HELPER REQUIRED_ARG)
    message(STATUS "helper arguments: ${REQUIRED_ARG}, followed by ${ARGV}")
    # set REQUIRED_ARG visible outside to make it as a return value
    set(${REQUIRED_ARG} "From helper" PARENT_SCOPE)

    # if can accept either var or ${var}
    # or some useful keywords:
    #     unary: NOT, TARGET, EXISTS(file), DEFINED, etc.
    #     binary: STREQUAL, AND, OR, MATCHES(regular exp), VERSION_LESS, VERSION_LESS_EQUAL, etc.
    #     parenthese for group
    if(variable)
        # 'ON' / 'TURE' / 'Y' / non-zero number
    else()
        # 'OFF' / 'FALSE' / 'N' / 0 / 'IGNORE' / 'NOTFOUND' / '""' / ends in '-NOTFOUND'
    endif()

    # test PARSE_ARG
    parse_arg(SINGLE ONE_VALUE one MULTI_VALUES one two three)
    # exprected result:
    # MY_PREFIX_SINGLE = TRUE
    # MY_PREFIX_ANOTHER = FALSE
    # MY_PREFIX_ONE_VALUE = "one"
    # MY_PREFIX_ALSO_ONE_VALUE = <UNDEFINED>
    # MY_PREFIX_MULTI_VALUES = "one;two;three"
endfunction()

######################## Project Contents ##################

# make a library
set{LIBRARY_NAME first}
add_library(${LIBRARY_NAME} STATIC) #option: STATIC, SHARED, MODULE
# can be set by BUILD_SHARED_LIBS (STATIC/SHARED)
# fintional target (e.g. header only): INTERFACE
# ALIAS: give a new name for an existing lib

# set multiple targets/files/tests at once
set_property(TARGET ${LIBRARY_NAME}
             PROPERTY CXX_STANDARD 20)
# shortcut for setting multiple properties on one target at once
set_target_properties(${LIBRARY_NAME} PROPERTIES
                      CXX_STANDARD 20)
# get properies
get_property(res TARGET ${LIBRARY_NAME} PROPERTY CXX_STANDARD)

# generator expressoin:
# $<KEYWORD>: evaluate to info relevant for the current config
# $<KEYWORD:value>: KEYWORD controls evaluation, value: the item to evaluate
#                   If KEYWORD is a generator expression or variable that evaluates to 0 or 1,
#                   value substituted if 1 / not if 0.
# common uses:
#   Limit an item to a certain language only;
#   Access configuration dependent properties;
#   Give different locations for build / install dirs.
target_compile_options(${LIBRARY_NAME} PRIVATE
                       "$<$<CONFIG:Debug>:--my-flag>")

# PUBLIC: any targets link to this target must also need that include
# PRIVATE: only affect current target, not dependencies
# INTERFACE: only needed for dependencies
target_include_directories(${LIBRARY_NAME} PUBLIC 
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                           $<INSTALL_INTERFACE:include>)

# run a command at build time
find_package(PythonInterp REQUIRED)
# use cmake -E <mode> for cross-plaform
# this allows CMake avoid calling system specific tools (e.g. mkdir)
set(GENERATE_HEADER_FILE "${CMAKE_BINARY_DIR}/include/Generated.hpp")
add_custom_command(OUTPUT ${GENERATE_HEADER_FILE}
                   COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/GenerateHeader.py"
                   DEPENDS ${LIBRARY_NAME}) # generate after ${LIBRARY_NAME} complete

add_custom_target(generate_header ALL # generate when `make` without `ALL`
                  DEPENDS ${GENERATE_HEADER_FILE})
# or add_dependencies(TARGET another_target generate_header)

install(FILES ${GENERATE_HEADER_FILE} DESTINATION include)

# make an executable
add_executable(my_proj) # TARGET name & file name
# chain targets
target_link_libraries(my_proj PUBLIC ${LIBRARY_NAME})
