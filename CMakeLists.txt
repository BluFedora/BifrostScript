################################################################################
#                                                                              #
#                           BIFROST SCRIPT PROJECT                             #
#                                                                              #
################################################################################
###  CMakeList.txt : Bifrost Script bytecode Virtual Machine project.        ###
################################################################################

cmake_minimum_required(VERSION 3.15)

project(BifrostScript VERSION 1.0.0 DESCRIPTION "This is a toy compiler and virtual machine for my simple scripting language." LANGUAGES C)

option(BIFROST_SCRIPT_SHARED_LIBRARY "Will build `BifrostScript` as a shared library" OFF)

if (BIFROST_SCRIPT_SHARED_LIBRARY)
  add_library(BifrostScript SHARED)

  target_compile_definitions(BifrostScript PRIVATE BF_VM_EXPORT_DYNAMIC)
else()
  add_library(BifrostScript STATIC)

  target_compile_definitions(BifrostScript PRIVATE BF_VM_EXPORT_STATIC)
endif()

if(MSVC)
  # /wd4200: nonstandard extension used: zero-sized array in struct/union.
  target_compile_options(BifrostScript PRIVATE /W4 /WX /wd4200)
else()
  target_compile_options(BifrostScript PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(MSVC)
  target_link_options(BifrostScript PRIVATE /incremental:no)
endif()

target_include_directories(
  BifrostScript
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_sources(
  BifrostScript
  PRIVATE
    "include/bifrost/bifrost_vm.h"
    "include/bifrost/bifrost_vm.hpp"
    
    "src/bifrost_hash_map.c"
    "src/bifrost_hash_map.c"
    "src/bifrost_vm_api.c"
    "src/bifrost_vm_debug.c"
    "src/bifrost_vm_debug.h"
    "src/bifrost_vm_function_builder.c"
    "src/bifrost_vm_function_builder.h"
    "src/bifrost_vm_gc.c"
    "src/bifrost_vm_gc.h"
    "src/bifrost_vm_instruction_op.h"
    "src/bifrost_vm_lexer.c"
    "src/bifrost_vm_lexer.h"
    "src/bifrost_vm_obj.c"
    "src/bifrost_vm_obj.h"
    "src/bifrost_vm_parser.c"
    "src/bifrost_vm_parser.h"
    "src/bifrost_vm_value.c"
    "src/bifrost_vm_value.h"
    "src/bifrost_libc.h" 
    "src/bifrost_libc.c")

set_target_properties(
  BifrostScript
  PROPERTIES
    C_STANDARD 99
)

set_target_properties(BifrostScript PROPERTIES OUTPUT_NAME                    BifrostScript)
set_target_properties(BifrostScript PROPERTIES VERSION                        ${PROJECT_VERSION})
set_target_properties(BifrostScript PROPERTIES PUBLIC_HEADER                  include/bifrost/script/bifrost_vm.h)
set_target_properties(BifrostScript PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# Command Line Interface

project(BifrostScript_cli DESCRIPTION "Command line interface for interacting with the vm.")

add_executable(
  BifrostScript_cli
  "vm_command_line.cpp"
)

target_link_libraries(
  BifrostScript_cli
  PRIVATE
    BifrostScript
)

set(CMAKE_BINARY_DIR       ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH    ${CMAKE_BINARY_DIR})

set_target_properties(
  BifrostScript_cli
  PROPERTIES
    CXX_STANDARD   17
    CXX_EXTENSIONS OFF
)

if(MSVC)
  target_link_options(BifrostScript_cli PRIVATE /incremental:no)
endif()
