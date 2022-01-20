cmake_minimum_required(VERSION 3.4.1)
set(CMAKE_CXX_STANDARD 14)
#添加内联支持
enable_language(C ASM)


include_directories(
        ./
        ./utils/
        ./netlink/
        ./Substrate/
)



find_library(
        log-lib
        log)

add_library(
        helloword

        SHARED

        main.cpp

        netlink/bionic_netlink.cpp
        netlink/ifaddrs.cpp

        #Substrate
        Substrate/hde64.c
        Substrate/SubstrateDebug.cpp
        Substrate/SubstrateHook.cpp
        Substrate/SubstratePosixMemory.cpp
        Substrate/And64InlineHook.cpp
)

target_link_libraries(
        helloword

        ${log-lib}
)




#---------------------初始化Dobby相关----------------------------------

#Dobby目录,从Git下载下来的zip压缩包,以后更新替换此压缩包即可。
#Dobby相对Substrate 来说Bug更少&支持短指令

#set(DobbyHome F:/Dobby)

#add_subdirectory(${DobbyHome} dobby)

#设置路径,引入Dobby
#include_directories(
#        ${DobbyHome}/include
#        ${DobbyHome}/source
#        ${DobbyHome}/builtin-plugin
#        ${DobbyHome}/builtin-plugin/AndroidRestriction
#        ${DobbyHome}/builtin-plugin/SymbolResolver
#        ${DobbyHome}/external/logging
#)
#
#macro(SET_OPTION option value)
#    set(${option} ${value} CACHE INTERNAL "" FORCE)
#endmacro()
#
#SET_OPTION(DOBBY_DEBUG ON)
## 关闭shared库,和我们的so 集成到一起。
#SET_OPTION(DOBBY_GENERATE_SHARED OFF)
#SET_OPTION(Plugin.LinkerLoadCallback OFF)
#
#
#add_subdirectory(${DobbyHome} dobby.build)

#---------------------初始化Dobby相关----------------------------------









