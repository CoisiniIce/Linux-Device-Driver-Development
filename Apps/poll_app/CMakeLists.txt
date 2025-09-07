cmake_minimum_required(VERSION 3.12)
set(CMAKE_C_LINK_DEPENDS_NO_SHARED "1")
project(01test)

# 指定编译器
set(CMAKE_C_COMPILER "/home/wang/ToolChain/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin/arm-buildroot-linux-gnueabihf-gcc")

set(BUILD_DIR ${PROJECT_SOURCE_DIR}/build)
set(INCLUDES_DIR ${PROJECT_SOURCE_DIR}/includes)
set(SRC_DIR ${PROJECT_SOURCE_DIR}/src)

# Output Path
set(EXECUTABLE_OUTPUT_PATH ${BUILD_DIR})

# Inlcudes Path
include_directories(${INCLUDES_DIR})

aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC_LIST)
add_executable(01test  ${SRC_LIST})
