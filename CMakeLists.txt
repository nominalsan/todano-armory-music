# SPDX-License-Identifier: GPL-3.0-or-later
# Todano Armory music (README.md): the mod's library (C, on the SDK's openrac/openrac_mod.h) and the
# test of its rules (the nearest preset and the entrance's entry watch) on memory of the test's own.
#
#   python3 tools/fetch_sdk.py                     # the SDK sdk.lock pins, into .sdk/
#   cmake -S . -B build -G Ninja
#   cmake --build build && ctest --test-dir build
#   python3 .sdk/openrac-mod-sdk-*/tools/package_mod.py build/package/todano_armory_music -o dist
cmake_minimum_required(VERSION 3.20)
project(todano_armory_music LANGUAGES C)

# The SDK: the release sdk.lock pins, in .sdk/ (tools/fetch_sdk.py). OPENRAC_SDK=<dir> or
# OpenRACMod_DIR=<dir>/cmake build against another.
if(NOT OpenRACMod_DIR)
  if(DEFINED OPENRAC_SDK)
    set(OpenRACMod_DIR "${OPENRAC_SDK}/cmake")
  else()
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/sdk.lock" lock)
    string(JSON sdk_version GET "${lock}" version)
    set(OpenRACMod_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.sdk/openrac-mod-sdk-${sdk_version}/cmake")
    if(NOT EXISTS "${OpenRACMod_DIR}/OpenRACModConfig.cmake")
      message(FATAL_ERROR "No OpenRAC mod SDK ${sdk_version}: run python3 tools/fetch_sdk.py first "
                          "(or pass -DOPENRAC_SDK=<an unpacked SDK>)")
    endif()
  endif()
endif()
find_package(OpenRACMod 1.14 REQUIRED)
include(CTest)

openrac_add_mod(todano_armory_music MANIFEST openrac-mod.json SOURCES src/todano_armory_music.c)
target_include_directories(todano_armory_music PRIVATE src)
set_target_properties(todano_armory_music PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
if(NOT MSVC)
  target_compile_options(todano_armory_music PRIVATE -Wall -Wextra)
endif()

if(BUILD_TESTING)
  add_executable(todano_armory_music_rules_test tests/rules_test.c)
  target_include_directories(todano_armory_music_rules_test PRIVATE src)
  set_target_properties(todano_armory_music_rules_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
  add_test(NAME todano_armory_music_rules COMMAND todano_armory_music_rules_test)
endif()
