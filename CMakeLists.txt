cmake_minimum_required(VERSION 2.8)

# Usage by vcpkg (CMAKE_TOOLCHAIN_FILE):
#   mkdir build && cd build && cmake .. -DCMAKE_TOOLCHAIN_FILE=$CMAKE_TOOLCHAIN_FILE
#   mkdir build && cd build && cmake .. -DCMAKE_TOOLCHAIN_FILE="/home/ubuntu/git-third/vcpkg/scripts/buildsystems/vcpkg.cmake"
#   cmake .. -DCMAKE_TOOLCHAIN_FILE=$CMAKE_TOOLCHAIN_FILE
#   cmake .. -DCMAKE_TOOLCHAIN_FILE="/home/ubuntu/git-third/vcpkg/scripts/buildsystems/vcpkg.cmake"

project(posix_quic)
set(TARGET ${CMAKE_PROJECT_NAME})

# flags
if (CMAKE_BUILD_TYPE)
else()
    set(CMAKE_BUILD_TYPE RELEASE)
endif()

message("------------ Options -------------")
message("  CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message("  CMAKE_COMMAND: ${CMAKE_COMMAND}")

set(CMAKE_CXX_FLAGS -std=c++11)
set(CMAKE_CXX_FLAGS_DEBUG "-g ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "-g -O3 ${CMAKE_CXX_FLAGS}")

message("------------ Cxx flags -------------")
message("  CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}: ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}}")
message("------------------------------------")

# source
aux_source_directory(${PROJECT_SOURCE_DIR}/src LIST_SRC)
add_library(${TARGET} STATIC ${LIST_SRC})

# include
include_directories(${PROJECT_SOURCE_DIR}/libquic)

add_subdirectory(${PROJECT_SOURCE_DIR}/libquic)

link_directories(${PROJECT_BIN_DIR}/libquic)

# target
target_link_libraries(${TARGET} quic -pthread -ldl)
