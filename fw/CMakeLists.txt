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
set(CMAKE_PROJECT_NAME DVM-V24)
set(CMAKE_TARGET_V1 dvm-v24-v1)
set(CMAKE_TARGET_V2 dvm-v24-v2)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Enable CMake support for ASM and C languages
enable_language(C ASM)

#
# Set GIT_VER compiler directive
#
set(GIT_VER "")
set(GIT_VER_HASH "UNKNOWN")
execute_process(COMMAND git describe --abbrev=8 --dirty --always WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} OUTPUT_VARIABLE GIT_VER OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND git describe --abbrev=8 --always WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} OUTPUT_VARIABLE GIT_VER_HASH OUTPUT_STRIP_TRAILING_WHITESPACE)
message("Git Version: " ${GIT_VER})
message("Git Hash:    " ${GIT_VER_HASH})

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Remap __FILE__ directive to display filename only, not full path
message("Remapping ${CMAKE_SOURCE_DIR}/ to /")
add_definitions(-fmacro-prefix-map="${CMAKE_SOURCE_DIR}/=/")

# Create executables
add_executable(${CMAKE_TARGET_V1})
add_executable(${CMAKE_TARGET_V2})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Common source files
set(V24_SOURCES 
    v24/src/fault.c
    v24/src/fifo.c
    v24/src/hdlc.c
    v24/src/log.c
    v24/src/serial.c
    v24/src/sync.c
    v24/src/util.c
    v24/src/vcp.c
)

# Common includes
set(V24_INCLUDE v24/inc)

#
#   V1 Target Setup
#
target_link_directories(${CMAKE_TARGET_V1} PRIVATE
)

target_sources(${CMAKE_TARGET_V1} PRIVATE
    ${V24_SOURCES}
)

target_include_directories(${CMAKE_TARGET_V1} PRIVATE
    ${V24_INCLUDE}
)

target_compile_definitions(${CMAKE_TARGET_V1} PRIVATE
    DVM_V24_V1
    GIT_HASH="${GIT_VER_HASH}"
)

# Add linked libraries
target_link_libraries(${CMAKE_TARGET_V1}
    stm32cubemx
)

# Add .bin generation
add_custom_command(TARGET ${CMAKE_TARGET_V1}
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_TARGET_V1}> ${CMAKE_TARGET_V1}.bin
)

#
#   V2 Target Setup
#
target_link_directories(${CMAKE_TARGET_V2} PRIVATE
)

target_sources(${CMAKE_TARGET_V2} PRIVATE
    ${V24_SOURCES}
)

target_include_directories(${CMAKE_TARGET_V2} PRIVATE
    ${V24_INCLUDE}
)

target_compile_definitions(${CMAKE_TARGET_V2} PRIVATE
    GIT_HASH="${GIT_VER_HASH}"
)

# Add linked libraries
target_link_libraries(${CMAKE_TARGET_V2}
    stm32cubemx
)

# Add .bin generation
add_custom_command(TARGET ${CMAKE_TARGET_V2}
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_TARGET_V2}> ${CMAKE_TARGET_V2}.bin
)

# Make sure bins are cleaned
set_property(
    DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    APPEND
    PROPERTY ADDITIONAL_CLEAN_FILES ${CMAKE_TARGET_V1}.bin ${CMAKE_TARGET_V2}.bin
)