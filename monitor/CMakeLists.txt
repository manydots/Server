# CMakeList.txt: server 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("server")
link_directories(${CMAKE_SOURCE_DIR}/lib)
include_directories(/usr/include/c++/4.8.2/i686-redhat-linux)
# set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ ")
# add_library(frida STATIC IMPORTED)
# set_target_properties(frida PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/libfrida-gum.a)

# add_library(vmp SHARED IMPORTED)
set(CMAKE_CXX_STANDARD 11)
# set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY /home/neople/monitor)

include_directories(inc)
add_library(fd_monitor SHARED         "monitor.cpp" "frida-gum.h"  "common.h" )
target_link_libraries(fd_monitor libfrida-gum.a pthread dl  )




# TODO: 如有需要，请添加测试并安装目标。
