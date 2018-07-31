#include "event.h"
#include <chrono>
#include <atomic>

namespace posix_quic {

void Event::EventTrigger::Wait(int timeout)
{
    std::unique_lock<std::mutex> lock(cvMtx);
    if (trigger) return ;
    if (timeout > 0) {
        cv.wait_for(lock, std::chrono::milliseconds(timeout));
    } else if (timeout == 0) {
        return ;
    } else {
        cv.wait(lock);
    }
}

void Event::EventTrigger::Trigger()
{
    std::unique_lock<std::mutex> lock(cvMtx);
    trigger = true;
    cv.notify_one();
}

int Event::StartWait(EventWaiter waiter, EventTrigger * trigger)
{
    std::unique_lock<std::mutex> lock(mtx_);
    if ((*waiter.events & POLLIN) && Readable())
        *waiter.revents |= POLLIN;
    if ((*waiter.events & POLLOUT) && Writable())
        *waiter.revents |= POLLOUT;
    if (Error())
        *waiter.revents |= POLLERR;

    waitings_[trigger] = waiter;
    return *waiter.revents;
}
void Event::Trigger(int event)
{
    std::unique_lock<std::mutex> lock(mtx_);
    for (auto & kv : waitings_) {
        EventTrigger * trigger = kv.first;
        EventWaiter & waiter = kv.second;
        switch (event) {
            case POLLIN:
                if (*waiter.events & POLLIN) {
                    std::__atomic_or_fetch(waiter.revents, POLLIN, std::memory_order_seq_cst);
                    trigger->Trigger();
                }
                break;

            case POLLOUT:
                if (*waiter.events & POLLOUT) {
                    std::__atomic_or_fetch(waiter.revents, POLLOUT, std::memory_order_seq_cst);
                    trigger->Trigger();
                }
                break;

            case POLLERR:
                std::__atomic_or_fetch(waiter.revents, POLLERR, std::memory_order_seq_cst);
                trigger->Trigger();
                break;

            default:
                break;
        }
    }
}

void Event::StopWait(EventTrigger * trigger)
{
    std::unique_lock<std::mutex> lock(mtx_);
    waitings_.erase(trigger);
}

void Event::SetReadable(bool b)
{
    readable = b;
    Trigger(POLLIN);
}
void Event::SetWritable(bool b)
{
    writable = b;
    Trigger(POLLOUT);
}
void Event::SetError(int err, int quicErr = 0)
{
    if (error == 0) {
        error = err;
        quicErrorCode = quicErr;
        Trigger(POLLERR);
    }
}

} // namespace posix_quic
