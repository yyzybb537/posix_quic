#pragma once
#include <atomic>

namespace posix_quic {

struct SpinLock
{
    std::atomic_flag flag;

    SpinLock() : flag{false}
    {
    }

    inline void lock()
    {
        while (flag.test_and_set(std::memory_order_acquire)) ;
    }

    inline bool try_lock()
    {
        return !flag.test_and_set(std::memory_order_acquire);
    }
    
    inline void unlock()
    {
        flag.clear(std::memory_order_release);
    }
};

} //namespace posix_quic
