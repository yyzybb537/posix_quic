INCLUDE += -I${POSIX_QUIC}/src
INCLUDE += -I${POSIX_QUIC}/libquic
INCLUDE += -I${POSIX_QUIC}/libquic/third_party/boringssl/src/include
INCLUDE += -I${POSIX_QUIC}/libquic/third_party/protobuf/src

LIB += ${POSIX_QUIC}/build/libposix_quic.a
LIB += ${POSIX_QUIC}/build/libquic/libquic.a
LIB += ${POSIX_QUIC}/build/libquic/third_party/boringssl/src/ssl/libssl.a
LIB += ${POSIX_QUIC}/build/libquic/third_party/boringssl/src/crypto/libcrypto.a
LIB += ${POSIX_QUIC}/build/libquic/third_party/boringssl/src/decrepit/libdecrepit.a
LIB += ${POSIX_QUIC}/build/libquic/third_party/protobuf/src/libprotobuf.a
