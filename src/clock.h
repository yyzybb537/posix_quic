#pragma once

#include "fwd.h"
#include "net/quic/quartc/quartc_clock_interface.h"

namespace posix_quic {

class QuicClockImpl
    : public net::QuartcClockInterface
{
public:
    static QuicClockImpl& getInstance();

    int64_t NowMicroseconds() override;

    int64_t NowMS() { return NowMicroseconds() / 1000; }

    QuicTime Now();
};

} // namespace posix_quic
