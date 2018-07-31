#include "clock.h"

namespace posix_quic {

int64_t QuicClockImpl::NowMicroseconds()
{
    struct timeval timeNow;
    gettimeofday(&timeNow, nullptr);
    int64_t timeNowUSecs = timeNow.tv_sec * 1000000LL + timeNow.tv_usec;
    return timeNowUSecs;
}

QuicTime QuicClockImpl::Now()
{
    return QuicTime::Zero() + QuicTime::Delta::FromMicroseconds(NowMicroseconds());
}

} // namespace posix_quic
