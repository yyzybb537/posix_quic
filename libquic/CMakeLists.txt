 cmake_minimum_required(VERSION 3.5.1)
 project(libquic)

 set(CMAKE_CXX_STANDARD 14)

# flags
if (CMAKE_BUILD_TYPE)
else()
    set(CMAKE_BUILD_TYPE RELEASE)
endif()

 if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
     message(STATUS "Setting build type to 'Debug' as none was specified.")
     set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
     # Set the possible values of build type for cmake-gui
     #set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
     #	"MinSizeRel" "RelWithDebInfo")
 endif ()

 if (WIN32)
     # On Windows, prefer cl over gcc if both are available. By default most of
     # the CMake generators prefer gcc, even on Windows.
     set(CMAKE_GENERATOR_CC cl)
 endif ()

 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")

 if (WIN32)
     if (CMAKE_COMPILER_IS_GNUCXX OR CLANG)
         # Note clang-cl is odd and sets both CLANG and MSVC. We base our configuration
         # primarily on our normal Clang one.
         set(C_CXX_FLAGS "-Werror -Wformat=2 -Wsign-compare -Wmissing-field-initializers -Wwrite-strings")
         if (MSVC)
             # clang-cl sets different default warnings than clang. It also treats -Wall
             # as -Weverything, to match MSVC. Instead -W3 is the alias for -Wall.
             # See http://llvm.org/viewvc/llvm-project?view=revision&revision=319116
             set(C_CXX_FLAGS "${C_CXX_FLAGS} -W3 -Wno-unused-parameter -fmsc-version=1900")
             # googletest suppresses warning C4996 via a pragma, but clang-cl does not
             # honor it. Suppress it here to compensate. See https://crbug.com/772117.
             set(C_CXX_FLAGS "${C_CXX_FLAGS} -Wno-deprecated-declarations")
         else ()
             set(C_CXX_FLAGS "${C_CXX_FLAGS} -Wall -ggdb -fvisibility=hidden -fno-common")
         endif ()

         if (CLANG)
             set(C_CXX_FLAGS "${C_CXX_FLAGS} -Wnewline-eof -fcolor-diagnostics")
         else ()
             # GCC (at least 4.8.4) has a bug where it'll find unreachable free() calls
             # and declare that the code is trying to free a stack pointer.
             set(C_CXX_FLAGS "${C_CXX_FLAGS} -Wno-free-nonheap-object")
         endif ()

         if (CLANG OR NOT "7.0.0" VERSION_GREATER CMAKE_C_COMPILER_VERSION)
             set(C_CXX_FLAGS "${C_CXX_FLAGS} -Wimplicit-fallthrough")
         endif ()

         set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_CXX_FLAGS} -Wmissing-prototypes -Wold-style-definition -Wstrict-prototypes")
         set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${C_CXX_FLAGS} -Wmissing-declarations")

         if (NOT MSVC)
             set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
             if (APPLE)
                 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
             endif ()
             if (NOT BORINGSSL_ALLOW_CXX_RUNTIME)
                 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
             endif ()
         endif ()

         # In GCC, -Wmissing-declarations is the C++ spelling of -Wmissing-prototypes
         # and using the wrong one is an error. In Clang, -Wmissing-prototypes is the
         # spelling for both and -Wmissing-declarations is some other warning.
         #
         # https://gcc.gnu.org/onlinedocs/gcc-7.1.0/gcc/Warning-Options.html#Warning-Options
         # https://clang.llvm.org/docs/DiagnosticsReference.html#wmissing-prototypes
         # https://clang.llvm.org/docs/DiagnosticsReference.html#wmissing-declarations
         if (CLANG)
             set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wmissing-prototypes")
         endif ()

         if (CMAKE_COMPILER_IS_GNUCXX AND "4.8" VERSION_GREATER CMAKE_C_COMPILER_VERSION)
             set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-array-bounds")
         endif ()

     elseif (MSVC)
         set(MSVC_DISABLED_WARNINGS_LIST
                 "C4061" # enumerator 'identifier' in switch of enum 'enumeration' is not
                 # explicitly handled by a case label
                 # Disable this because it flags even when there is a default.
                 "C4100" # 'exarg' : unreferenced formal parameter
                 "C4127" # conditional expression is constant
                 "C4200" # nonstandard extension used : zero-sized array in
                 # struct/union.
                 "C4204" # nonstandard extension used: non-constant aggregate initializer
                 "C4221" # nonstandard extension used : 'identifier' : cannot be
                 # initialized using address of automatic variable
                 "C4242" # 'function' : conversion from 'int' to 'uint8_t',
                 # possible loss of data
                 "C4244" # 'function' : conversion from 'int' to 'uint8_t',
                 # possible loss of data
                 "C4267" # conversion from 'size_t' to 'int', possible loss of data
                 "C4371" # layout of class may have changed from a previous version of the
                 # compiler due to better packing of member '...'
                 "C4388" # signed/unsigned mismatch
                 "C4296" # '>=' : expression is always true
                 "C4350" # behavior change: 'std::_Wrap_alloc...'
                 "C4365" # '=' : conversion from 'size_t' to 'int',
                 # signed/unsigned mismatch
                 "C4389" # '!=' : signed/unsigned mismatch
                 "C4464" # relative include path contains '..'
                 "C4510" # 'argument' : default constructor could not be generated
                 "C4512" # 'argument' : assignment operator could not be generated
                 "C4514" # 'function': unreferenced inline function has been removed
                 "C4548" # expression before comma has no effect; expected expression with
                 # side-effect" caused by FD_* macros.
                 "C4610" # struct 'argument' can never be instantiated - user defined
                 # constructor required.
                 "C4623" # default constructor was implicitly defined as deleted
                 "C4625" # copy constructor could not be generated because a base class
                 # copy constructor is inaccessible or deleted
                 "C4626" # assignment operator could not be generated because a base class
                 # assignment operator is inaccessible or deleted
                 "C4668" # 'symbol' is not defined as a preprocessor macro, replacing with
                 # '0' for 'directives'
                 # Disable this because GTest uses it everywhere.
                 "C4706" # assignment within conditional expression
                 "C4710" # 'function': function not inlined
                 "C4711" # function 'function' selected for inline expansion
                 "C4800" # 'int' : forcing value to bool 'true' or 'false'
                 # (performance warning)
                 "C4820" # 'bytes' bytes padding added after construct 'member_name'
                 "C5026" # move constructor was implicitly defined as deleted
                 "C5027" # move assignment operator was implicitly defined as deleted
                 "C4996"
                 "C4582"
                 "C4355"
                 "C4457"
                 "C4826"
                 "C4265"
                 "C4456"
                 "C4266"
                 "C4191"
                 "C4067"
                 "C4701"
                 "C4738"
                 "C4647"
                 "C4819"
                 )
         set(MSVC_LEVEL4_WARNINGS_LIST
                 # See https://connect.microsoft.com/VisualStudio/feedback/details/1217660/warning-c4265-when-using-functional-header
                 "C4265" # class has virtual functions, but destructor is not virtual
                 )
         string(REPLACE "C" " -wd" MSVC_DISABLED_WARNINGS_STR
                 ${MSVC_DISABLED_WARNINGS_LIST})
         string(REPLACE "C" " -w4" MSVC_LEVEL4_WARNINGS_STR
                 ${MSVC_LEVEL4_WARNINGS_LIST})
         set(CMAKE_C_FLAGS "-utf-8 -Wall -WX ${MSVC_DISABLED_WARNINGS_STR} ${MSVC_LEVEL4_WARNINGS_STR}")
         set(CMAKE_CXX_FLAGS "-utf-8 -Wall -WX ${MSVC_DISABLED_WARNINGS_STR} ${MSVC_LEVEL4_WARNINGS_STR}")
     endif ()
 endif ()

 if (WIN32)
     add_definitions(-D_HAS_EXCEPTIONS=0)
     add_definitions(-DWIN32_LEAN_AND_MEAN)
     add_definitions(-DNOMINMAX)
     add_definitions(-DUNICODE)
     # Allow use of fopen.
     add_definitions(-D_CRT_SECURE_NO_WARNINGS)
     # VS 2017 and higher supports STL-only warning suppressions.
     add_definitions("-D_STL_EXTRA_DISABLED_WARNINGS=4774 4987")
     add_definitions("-D_SCL_SECURE_NO_WARNINGS")
     add_definitions(-w)
     add_definitions(-DWIN32)
 else ()
     set(COMMON_C_FLAGS "${COMMON_C_FLAGS} -Wall -fpic")
     set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_C_FLAGS}")
     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_C_FLAGS} -std=gnu++11")
     set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG -O3")
     set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O3")
     set(CMAKE_C_FLAGS_DEBUG "-g")
     set(CMAKE_CXX_FLAGS_DEBUG "-g")
 endif ()

 add_definitions(-DUSE_OPENSSL -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS)

 if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
     set(ARCH "amd64")
 elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "amd64")
     set(ARCH "amd64")
 elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64")
     # cmake reports AMD64 on Windows, but we might be building for 32-bit.
     if (CMAKE_CL_64)
         set(ARCH "amd64")
     else ()
         set(ARCH "386")
     endif ()
 elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86")
     set(ARCH "386")
 elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "i386")
     set(ARCH "386")
 elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "i686")
     set(ARCH "386")
 elseif (${CMAKE_SYSTEM_PROCESSOR} MATCHES "^arm.*")
     set(ARCH "arm")
 elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
     set(ARCH "aarch64")
 else ()
     message(FATAL_ERROR "Unknown processor:" ${CMAKE_SYSTEM_PROCESSOR})
 endif ()

 if (${ARCH} STREQUAL "386" AND APPLE)
     # With CMake 2.8.x, ${CMAKE_SYSTEM_PROCESSOR} evalutes to i386 on OS X,
     # but clang defaults to 64-bit builds on OS X unless otherwise told.
     # Set ARCH to x86_64 so clang and CMake agree. This is fixed in CMake 3.
     set(ARCH "amd64")
 endif ()

 include_directories(.)
 include_directories(third_party/boringssl/src/include)
 include_directories(third_party/protobuf/src)

 add_subdirectory(third_party/boringssl/src)
 add_subdirectory(third_party/protobuf/src)

 set(
         NET_SOURCES
         net/base/address_family.cc
         net/base/arena.cc
         net/base/escape.cc
         net/base/hex_utils.cc
         net/base/host_port_pair.cc
         net/base/int128.cc
         net/base/io_buffer.cc
         net/base/ip_address.cc
         net/base/ip_endpoint.cc
         net/base/lookup_string_in_fixed_set.cc
         net/base/net_errors.cc
         net/base/net_module.cc
         net/base/parse_number.cc
         net/base/port_util.cc
         net/base/sockaddr_storage.cc
         net/base/url_util.cc
         net/quic/chromium/quic_utils_chromium.cc
         net/quic/core/congestion_control/bandwidth_sampler.cc
         net/quic/core/congestion_control/bbr_sender.cc
         net/quic/core/congestion_control/cubic_bytes.cc
         net/quic/core/congestion_control/general_loss_algorithm.cc
         net/quic/core/congestion_control/hybrid_slow_start.cc
         net/quic/core/congestion_control/pacing_sender.cc
         net/quic/core/congestion_control/prr_sender.cc
         net/quic/core/congestion_control/rtt_stats.cc
         net/quic/core/congestion_control/send_algorithm_interface.cc
         net/quic/core/congestion_control/tcp_cubic_sender_bytes.cc
         net/quic/core/crypto/aead_base_decrypter.cc
         net/quic/core/crypto/aead_base_encrypter.cc
         net/quic/core/crypto/aes_128_gcm_12_decrypter.cc
         net/quic/core/crypto/aes_128_gcm_12_encrypter.cc
         net/quic/core/crypto/aes_128_gcm_decrypter.cc
         net/quic/core/crypto/aes_128_gcm_encrypter.cc
         net/quic/core/crypto/aes_256_gcm_decrypter.cc
         net/quic/core/crypto/aes_256_gcm_encrypter.cc
         net/quic/core/crypto/cert_compressor.cc
         net/quic/core/crypto/chacha20_poly1305_decrypter.cc
         net/quic/core/crypto/chacha20_poly1305_encrypter.cc
         net/quic/core/crypto/chacha20_poly1305_tls_decrypter.cc
         net/quic/core/crypto/chacha20_poly1305_tls_encrypter.cc
         net/quic/core/crypto/channel_id.cc
         net/quic/core/crypto/common_cert_set.cc
         net/quic/core/crypto/common_cert_set_2.c
         net/quic/core/crypto/common_cert_set_2a.inc
         net/quic/core/crypto/common_cert_set_2b.inc
         net/quic/core/crypto/common_cert_set_3.c
         net/quic/core/crypto/common_cert_set_3a.inc
         net/quic/core/crypto/common_cert_set_3b.inc
         net/quic/core/crypto/crypto_framer.cc
         net/quic/core/crypto/crypto_handshake.cc
         net/quic/core/crypto/crypto_handshake_message.cc
         net/quic/core/crypto/crypto_secret_boxer.cc
         net/quic/core/crypto/crypto_server_config_protobuf.cc
         net/quic/core/crypto/crypto_utils.cc
         net/quic/core/crypto/curve25519_key_exchange.cc
         net/quic/core/crypto/null_decrypter.cc
         net/quic/core/crypto/null_encrypter.cc
         net/quic/core/crypto/p256_key_exchange.cc
         net/quic/core/crypto/proof_source.cc
         net/quic/core/crypto/quic_compressed_certs_cache.cc
         net/quic/core/crypto/quic_crypto_client_config.cc
         net/quic/core/crypto/quic_crypto_proof.cc
         net/quic/core/crypto/quic_crypto_server_config.cc
         net/quic/core/crypto/quic_decrypter.cc
         net/quic/core/crypto/quic_encrypter.cc
         net/quic/core/crypto/quic_random.cc
         net/quic/core/crypto/quic_tls_adapter.cc
         net/quic/core/crypto/scoped_evp_aead_ctx.cc
         net/quic/core/frames/quic_ack_frame.cc
         net/quic/core/frames/quic_blocked_frame.cc
         net/quic/core/frames/quic_connection_close_frame.cc
         net/quic/core/frames/quic_frame.cc
         net/quic/core/frames/quic_goaway_frame.cc
         net/quic/core/frames/quic_ietf_blocked_frame.cc
         net/quic/core/frames/quic_ietf_max_stream_id_frame.cc
         net/quic/core/frames/quic_ietf_stream_id_blocked_frame.cc
         net/quic/core/frames/quic_padding_frame.cc
         net/quic/core/frames/quic_path_challenge_frame.cc
         net/quic/core/frames/quic_path_response_frame.cc
         net/quic/core/frames/quic_ping_frame.cc
         net/quic/core/frames/quic_rst_stream_frame.cc
         net/quic/core/frames/quic_stop_sending_frame.cc
         net/quic/core/frames/quic_stop_waiting_frame.cc
         net/quic/core/frames/quic_stream_frame.cc
         net/quic/core/frames/quic_window_update_frame.cc
         net/quic/core/quic_ack_listener_interface.cc
         net/quic/core/quic_alarm.cc
         net/quic/core/quic_bandwidth.cc
         net/quic/core/quic_buffer_allocator.cc
         net/quic/core/quic_config.cc
         net/quic/core/quic_connection.cc
         net/quic/core/quic_connection_stats.cc
         net/quic/core/quic_constants.cc
         net/quic/core/quic_control_frame_manager.cc
         net/quic/core/quic_crypto_client_handshaker.cc
         net/quic/core/quic_crypto_client_stream.cc
         net/quic/core/quic_crypto_handshaker.cc
         net/quic/core/quic_crypto_server_handshaker.cc
         net/quic/core/quic_crypto_server_stream.cc
         net/quic/core/quic_crypto_stream.cc
         net/quic/core/quic_data_reader.cc
         net/quic/core/quic_data_writer.cc
         net/quic/core/quic_error_codes.cc
         net/quic/core/quic_flow_controller.cc
         net/quic/core/quic_framer.cc
         net/quic/core/quic_packet_creator.cc
         net/quic/core/quic_packet_generator.cc
         net/quic/core/quic_packets.cc
         net/quic/core/quic_received_packet_manager.cc
         net/quic/core/quic_sent_packet_manager.cc
         net/quic/core/quic_server_id.cc
         net/quic/core/quic_session.cc
         net/quic/core/quic_simple_buffer_allocator.cc
         net/quic/core/quic_socket_address_coder.cc
         net/quic/core/quic_stream.cc
         net/quic/core/quic_stream_send_buffer.cc
         net/quic/core/quic_stream_sequencer.cc
         net/quic/core/quic_stream_sequencer_buffer.cc
         net/quic/core/quic_sustained_bandwidth_recorder.cc
         net/quic/core/quic_tag.cc
         net/quic/core/quic_time.cc
         net/quic/core/quic_transmission_info.cc
         net/quic/core/quic_types.cc
         net/quic/core/quic_unacked_packet_map.cc
         net/quic/core/quic_utils.cc
         net/quic/core/quic_versions.cc
         net/quic/core/quic_write_blocked_list.cc
         net/quic/core/tls_client_handshaker.cc
         net/quic/core/tls_handshaker.cc
         net/quic/core/tls_server_handshaker.cc
         net/quic/core/proto/source_address_token.pb.cc
         net/quic/core/proto/cached_network_parameters.pb.cc
         net/quic/platform/api/quic_clock.cc
         net/quic/platform/api/quic_hostname_utils.cc
         net/quic/platform/api/quic_ip_address.cc
         net/quic/platform/api/quic_mutex.cc
         net/quic/platform/api/quic_socket_address.cc
         net/quic/platform/impl/quic_flags_impl.cc
         net/quic/platform/impl/quic_hostname_utils_impl.cc
         net/quic/platform/impl/quic_ip_address_impl.cc
         net/quic/platform/impl/quic_mem_slice_impl.cc
         net/quic/platform/impl/quic_mem_slice_span_impl.cc
         net/quic/platform/impl/quic_mutex_impl.cc
         net/quic/platform/impl/quic_socket_address_impl.cc
         net/quic/quartc/quartc_factory.cc
         net/quic/quartc/quartc_factory_interface.cc
         net/quic/quartc/quartc_packet_writer.cc
         net/quic/quartc/quartc_session.cc
         net/quic/quartc/quartc_stream.cc
         net/spdy/core/spdy_alt_svc_wire_format.cc
         net/spdy/core/spdy_header_block.cc
         net/spdy/core/spdy_protocol.cc
         net/spdy/platform/impl/spdy_mem_slice_impl.cc
         net/spdy/platform/impl/spdy_string_utils_impl.cc
 )

 set(
         BASE_COMMON
         base/logging.cc
         base/memory/ref_counted.cc
         base/pickle.cc
         base/strings/string_number_conversions.cc
         base/strings/string_piece.cc
         base/strings/string_split.cc
         base/strings/string_util.cc
         base/strings/stringprintf.cc
         base/strings/utf_offset_string_conversions.cc
         base/strings/utf_string_conversion_utils.cc
         base/strings/utf_string_conversions.cc
         base/strings/string_util_constants.cc
         base/synchronization/lock.cc
         base/trace_event/memory_usage_estimator.cc
         base/lazy_instance_helpers.cc
         base/callback_internal.cc
         base/time/time.cc
         base/time/time_override.cc
         base/rand_util.cc
         base/threading/thread_local_storage.cc
         base/at_exit.cc
         base/third_party/icu/icu_utf.cc
         base/third_party/dmg_fp/dtoa.cc
         base/third_party/dmg_fp/g_fmt.cc
         base/third_party/nspr/prtime.cc
 )

 set(
         BASE_PLATFORM_POSIX
         base/time/time_exploded_posix.cc
         base/time/time_conversion_posix.cc
         base/synchronization/lock_impl_posix.cc
         base/posix/safe_strerror.cc
         base/rand_util_posix.cc
         base/threading/thread_local_storage_posix.cc
         net/base/net_errors_posix.cc
         base/strings/string16.cc
 )

 set(
         URL_SOURCES
         url/third_party/mozilla/url_parse.cc
         url/gurl.cc
         url/origin.cc
         url/scheme_host_port.cc
         url/url_canon.cc
         url/url_canon_host.cc
         url/url_canon_internal.cc
         url/url_canon_ip.cc
         url/url_canon_stdstring.cc
         url/url_constants.cc
         url/url_util.cc
         url/url_canon_stdurl.cc
         url/url_canon_etc.cc
         url/url_canon_relative.cc
         url/url_canon_query.cc
 )

 set(
         CRYPTO_SOURCES
         crypto/hkdf.cc
         crypto/random.cc
         crypto/hmac.cc
         crypto/secure_util.cc
 )

 set(
         ZLIB_SOURCES
         third_party/zlib/gzwrite.c
         third_party/zlib/crc32.c
         third_party/zlib/zutil.c
         third_party/zlib/deflate.c
         third_party/zlib/simd_stub.c
         third_party/zlib/infback.c
         third_party/zlib/inffast.c
         third_party/zlib/inflate.c
         third_party/zlib/inftrees.c
         third_party/zlib/compress.c
         third_party/zlib/gzclose.c
         third_party/zlib/adler32.c
         third_party/zlib/gzlib.c
         third_party/zlib/trees.c
         third_party/zlib/gzread.c
         third_party/zlib/uncompr.c
 )

 if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
     set(
             BASE_ARCH_LIBRARIES
     )
     set(BASE_PLATFORM
             ${BASE_PLATFORM_POSIX}
             base/time/time_now_posix.cc
             )

 elseif (${CMAKE_SYSTEM_NAME} MATCHES "Android")
     set(
             BASE_ARCH_LIBRARIES
     )
     set(BASE_PLATFORM
             base/time/time_android.cc
             base/time/time_now_posix.cc
             ${BASE_PLATFORM_POSIX})

 elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

     FIND_LIBRARY(COREFOUNDATION_LIBRARY CoreFoundation)
     FIND_LIBRARY(COCOA_LIBRARY Cocoa)
     FIND_LIBRARY(SECURITY_LIBRARY Security)

     try_compile(CMAKE_SYSTEM_IS_IOS ${CMAKE_CURRENT_BINARY_DIR}/is_ios
             SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/is_ios.c
             OUTPUT_VARIABLE IOS_TEST)

     set(
             BASE_ARCH_LIBRARIES
             ${COREFOUNDATION_LIBRARY}
             ${COCOA_LIBRARY}
     )

     if (NOT CMAKE_SYSTEM_IS_IOS)
         list(APPEND BASE_ARCH_LIBRARIES ${SECURITY_LIBRARY})
     endif ()

     set(BASE_PLATFORM
             base/mac/mach_logging.cc
             base/mac/scoped_mach_port.cc
             base/time/time_mac.cc
             ${BASE_PLATFORM_POSIX})

 elseif (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
     set(
             BASE_ARCH_LIBRARIES
     )
     set(BASE_PLATFORM
             net/base/net_errors_win.cc
             base/synchronization/lock_impl_win.cc
             base/threading/thread_local_storage_win.cc
             base/rand_util_win.cc
             base/time/time_win.cc
             base/cpu.cc
             )

 endif ()

 add_library(quic STATIC
         ${NET_SOURCES}
         ${BASE_COMMON}
         ${BASE_PLATFORM}
         ${URL_SOURCES}
         ${CRYPTO_SOURCES}
         ${ZLIB_SOURCES})

 target_link_libraries(quic protobuf ssl crypto ${BASE_ARCH_LIBRARIES})
