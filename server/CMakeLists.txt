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
# set_target_properties(vmp PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/libVMProtectSDK32.so)
set(CMAKE_CXX_STANDARD 11)
# set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY /home/neople/game)
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto")
# 将源代码添加到此项目的可执行文件。

include_directories(inc)
add_library(fd SHARED  "server.cpp" "server.h" "frida-gum.h" "mysql.cpp" "common.h" )
target_link_libraries(fd libfrida-gum.a pthread dl )

#set_target_properties(fd PROPERTIES LINK_FLAGS "-Wl,-Bstatic") # 添加链接器标志
#set_target_properties(fd PROPERTIES C_VISIBILITY_DEFAULT "hidden")
#set_target_properties(fd PROPERTIES CXX_VISIBILITY_DEFAULT  "hidden")

# if (CMAKE_VERSION VERSION_GREATER 3.12)
#   set_property(TARGET server PROPERTY CXX_STANDARD 20)
# endif()

# TODO: 如有需要，请添加测试并安装目标。
