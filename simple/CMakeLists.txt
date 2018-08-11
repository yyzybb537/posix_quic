cmake_minimum_required(VERSION 3.5.1)

set(CMAKE_CXX_STANDARD 14)

# Usage by vcpkg (CMAKE_TOOLCHAIN_FILE):
#   mkdir build && cd build && cmake .. -DCMAKE_TOOLCHAIN_FILE=$CMAKE_TOOLCHAIN_FILE
#   mkdir build && cd build && cmake .. -DCMAKE_TOOLCHAIN_FILE="/home/ubuntu/git-third/vcpkg/scripts/buildsystems/vcpkg.cmake"
#   cmake .. -DCMAKE_TOOLCHAIN_FILE=$CMAKE_TOOLCHAIN_FILE
#   cmake .. -DCMAKE_TOOLCHAIN_FILE="/home/ubuntu/git-third/vcpkg/scripts/buildsystems/vcpkg.cmake"

project(simple_quic)
set(TARGET simple_quic)

# flags
if (CMAKE_BUILD_TYPE)
else()
    set(CMAKE_BUILD_TYPE RELEASE)
endif()

message("------------ Options -------------")
message("  CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message("  CMAKE_COMMAND: ${CMAKE_COMMAND}")

set(CMAKE_CXX_FLAGS_DEBUG "-g -fno-omit-frame-pointer ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "-g -O3 -fno-omit-frame-pointer ${CMAKE_CXX_FLAGS}")

message("------------ Cxx flags -------------")
message("  CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}: ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}}")
message("------------------------------------")

# source
aux_source_directory(${PROJECT_SOURCE_DIR} LIST_SRC)

# include
#include_directories(${PROJECT_SOURCE_DIR})
#include_directories(${PROJECT_SOURCE_DIR}/../src)
#include_directories(${PROJECT_SOURCE_DIR}/../libquic)
#include_directories(${PROJECT_SOURCE_DIR}/../libquic/third_party/boringssl/src/include)
#include_directories(${PROJECT_SOURCE_DIR}/../libquic/third_party/protobuf/src)
#
#link_directories(${PROJECT_SOURCE_DIR}/../build)
#link_directories(${PROJECT_SOURCE_DIR}/../build/libquic)
#link_directories(${PROJECT_SOURCE_DIR}/../build/libquic/third_party/boringssl/src/ssl)
#link_directories(${PROJECT_SOURCE_DIR}/../build/libquic/third_party/boringssl/src/crypto)
#link_directories(${PROJECT_SOURCE_DIR}/../build/libquic/third_party/boringssl/src/decrepit)
#link_directories(${PROJECT_SOURCE_DIR}/../build/libquic/third_party/protobuf/src)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/../src)
include_directories(${PROJECT_SOURCE_DIR}/../libquic)
include_directories(${PROJECT_SOURCE_DIR}/../libquic/third_party/boringssl/src/include)
include_directories(${PROJECT_SOURCE_DIR}/../libquic/third_party/protobuf/src)

# target
add_library(${TARGET} STATIC ${LIST_SRC})
