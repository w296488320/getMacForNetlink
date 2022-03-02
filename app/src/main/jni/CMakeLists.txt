cmake_minimum_required(VERSION 3.4.1)
set(CMAKE_CXX_STANDARD 14)
#添加内联支持
enable_language(C ASM)

#开始优化
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O")
# inline
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden")


include_directories(
        ./
        ./utils/
        ./netlink/
        ./Substrate/
)

if (${CMAKE_ANDROID_ARCH_ABI} STREQUAL "arm64-v8a")
    set(syscall netlink/syscall64.s)
elseif (${CMAKE_ANDROID_ARCH_ABI} STREQUAL "armeabi-v7a")
    set(syscall netlink/syscall32.s)
endif ()

find_library(
        log-lib
        log
)

add_library(
        helloword

        SHARED

        main.cpp

        netlink/bionic_netlink.cpp
        netlink/ifaddrs.cpp

        ${syscall}
)

target_link_libraries(
        helloword

        ${log-lib}
)










