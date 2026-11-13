# SPDX-License-Identifier: GPL-3.0-or-later
# Cross-compiles the mod for Windows x86_64 from Linux with MinGW-w64 (README.md, "Build"):
#
#   cmake -S . -B build-windows -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
#
# libgcc is linked in statically, so the DLL needs only Windows' own KERNEL32.dll and msvcrt.dll.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
# The OpenRAC SDK is found from the host's file system (sdk.lock's .sdk/, or OPENRAC_SDK).
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-static-libgcc")
