cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME ETH_Demo01)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# 设置 三方目录 目录
set(Third_Party_Dir "${CMAKE_CURRENT_SOURCE_DIR}/Third_Party")

# 借用三方的 .cmake
include(${Third_Party_Dir}/LwIP/src/Filelists.cmake)

# 设置 FreeRTOS 源文件
set(FreeRTOS_Src
    ${Third_Party_Dir}/FreeRTOS/source/croutine.c
    ${Third_Party_Dir}/FreeRTOS/source/event_groups.c
    ${Third_Party_Dir}/FreeRTOS/source/list.c
    ${Third_Party_Dir}/FreeRTOS/source/queue.c
    ${Third_Party_Dir}/FreeRTOS/source/stream_buffer.c
    ${Third_Party_Dir}/FreeRTOS/source/tasks.c
    ${Third_Party_Dir}/FreeRTOS/source/timers.c
    ${Third_Party_Dir}/FreeRTOS/portable/MemMang/heap_4.c
    ${Third_Party_Dir}/FreeRTOS/portable/ARM_CM4F/port.c
)

# 设置 LwIP 源文件
set(LwIP_Src
    ${lwipnetif_SRCS}
    ${lwipapi_SRCS}
    ${lwipcore_SRCS}
    ${lwipcore4_SRCS}
)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${CMAKE_SOURCE_DIR}/LWIP/App/lwip.c
    ${CMAKE_SOURCE_DIR}/LWIP/Target/ethernetif.c
    ${CMAKE_SOURCE_DIR}/LWIP/Target/sys_arch.c
    ${CMAKE_SOURCE_DIR}/Drivers/BSP/Components/lan8742/lan8742.c
)

# 三方部分编入源码
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${FreeRTOS_Src})
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${LwIP_Src})

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    # FreeRTOS 部分
    ${Third_Party_Dir}/FreeRTOS/portable/ARM_CM4F
    ${Third_Party_Dir}/FreeRTOS/include
    ${Third_Party_Dir}/FreeRTOS
    # LwIP 部分
    ${CMAKE_SOURCE_DIR}/LWIP/App
    ${CMAKE_SOURCE_DIR}/LWIP/Target
    ${Third_Party_Dir}/LwIP/src/include
    ${Third_Party_Dir}/LwIP/system
    ${Third_Party_Dir}/LwIP/src/include/netif/ppp
    ${Third_Party_Dir}/LwIP/src/include/lwip
    ${Third_Party_Dir}/LwIP/src/include/lwip/apps
    ${Third_Party_Dir}/LwIP/src/include/lwip/priv
    ${Third_Party_Dir}/LwIP/src/include/lwip/prot
    ${Third_Party_Dir}/LwIP/src/include/netif
    ${Third_Party_Dir}/LwIP/src/include/compat/posix
    ${Third_Party_Dir}/LwIP/src/include/compat/posix/arpa
    ${Third_Party_Dir}/LwIP/src/include/compat/posix/net
    ${Third_Party_Dir}/LwIP/src/include/compat/posix/sys
    ${Third_Party_Dir}/LwIP/src/include/compat/stdc
    ${Third_Party_Dir}/LwIP/system/arch
    # LAN8742
    ${CMAKE_SOURCE_DIR}/Drivers/BSP/Components/lan8742
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
