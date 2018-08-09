#pragma once

#include <memory>
#include "option.h"
#include "constants.h"

namespace posix_quic {

#define INVALID_QUIC_CONNECTION_ID QuicConnectionId(-1)

typedef int UdpSocket;
typedef int QuicSocket;
typedef int QuicStream;

enum class EntryCategory : int8_t {
    Invalid,
    Socket,
    Stream,
};

} // namespace posix_quic
