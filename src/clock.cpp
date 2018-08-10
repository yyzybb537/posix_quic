#include "clock.h"
#include "clock_impl.h"

namespace posix_quic {

static int ClockInitialize() {
    std::thread(&FastSteadyClock::ThreadRun).detach();
    return 0;
}

QuicClockImpl& QuicClockImpl::getInstance()
{
    static int ign = ClockInitialize();
    (void)ign;

    static QuicClockImpl obj;
    return obj;
}

int64_t QuicClockImpl::NowMicroseconds()
{
    auto tp = FastSteadyClock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count();
}

QuicTime QuicClockImpl::Now()
{
    return QuicTime::Zero() + QuicTime::Delta::FromMicroseconds(NowMicroseconds());
}

} // namespace posix_quic
