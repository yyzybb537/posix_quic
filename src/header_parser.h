#pragma once

#include "fwd.h"
#include "net/quic/core/quic_framer.h"
#include <mutex>

namespace posix_quic {

class HeaderParser
{
public:
    static QuicConnectionId ParseHeader(const char* data, size_t len);
};

} // namespace posix_quic
