# Minimum CMake required
cmake_minimum_required(VERSION 2.8.12)

if (protobuf_VERBOSE)
    message(STATUS "Protocol Buffers Configuring...")
endif ()

# CMake policies
cmake_policy(SET CMP0022 NEW)

if (POLICY CMP0048)
    cmake_policy(SET CMP0048 NEW)
endif ()

# Project
project(libprotobuf C CXX)

# Add c++11 flags for clang
set(CMAKE_CXX_FLAGS "-std=c++11")

if (MSVC)
    # Build with multiple processes
    add_definitions(/MP)
    # MSVC warning suppressions
    add_definitions(
            /wd4018 # 'expression' : signed/unsigned mismatch
            /wd4065 # switch statement contains 'default' but no 'case' labels
            /wd4146 # unary minus operator applied to unsigned type, result still unsigned
            /wd4244 # 'conversion' conversion from 'type1' to 'type2', possible loss of data
            /wd4251 # 'identifier' : class 'type' needs to have dll-interface to be used by clients of class 'type2'
            /wd4267 # 'var' : conversion from 'size_t' to 'type', possible loss of data
            /wd4305 # 'identifier' : truncation from 'type1' to 'type2'
            /wd4307 # 'operator' : integral constant overflow
            /wd4309 # 'conversion' : truncation of constant value
            /wd4334 # 'operator' : result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
            /wd4355 # 'this' : used in base member initializer list
            /wd4506 # no definition for inline function 'function'
            /wd4800 # 'type' : forcing value to bool 'true' or 'false' (performance warning)
            /wd4996 # The compiler encountered a deprecated declaration.
    )
    # Suppress linker warnings about files with no symbols defined.
    set(CMAKE_STATIC_LINKER_FLAGS /ignore:4221)
endif (MSVC)


include_directories(.)

if (NOT ANDROID)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -DHAVE_PTHREAD")
else ()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_PTHREAD")
endif ()

add_library(
        protobuf
        STATIC

        google/protobuf/arena.cc
        google/protobuf/arenastring.cc
        google/protobuf/extension_set.cc
        google/protobuf/generated_message_util.cc
        google/protobuf/io/coded_stream.cc
        google/protobuf/io/zero_copy_stream.cc
        google/protobuf/io/zero_copy_stream_impl_lite.cc
        google/protobuf/message_lite.cc
        google/protobuf/repeated_field.cc
        google/protobuf/stubs/atomicops_internals_x86_gcc.cc
        google/protobuf/stubs/atomicops_internals_x86_msvc.cc
        google/protobuf/stubs/bytestream.cc
        google/protobuf/stubs/common.cc
        google/protobuf/stubs/int128.cc
        google/protobuf/stubs/once.cc
        google/protobuf/stubs/status.cc
        google/protobuf/stubs/statusor.cc
        google/protobuf/stubs/stringpiece.cc
        google/protobuf/stubs/stringprintf.cc
        google/protobuf/stubs/structurally_valid.cc
        google/protobuf/stubs/strutil.cc
        google/protobuf/stubs/time.cc
        google/protobuf/wire_format_lite.cc
)
