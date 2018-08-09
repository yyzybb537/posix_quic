#include "event.h"
#include <chrono>
#include <atomic>
#include <poll.h>
#include "epoller_entry.h"

namespace posix_quic {

void Event::EventTrigger::Wait(int timeout)
{
    std::unique_lock<std::mutex> lock(cvMtx);
    if (triggered) {
        triggered = false;
        return ;
    }
    if (timeout > 0) {
        cv.wait_for(lock, std::chrono::milliseconds(timeout));
    } else if (timeout == 0) {
        return ;
    } else {
        cv.wait(lock);
    }
}

void Event::EventTrigger::Trigger(short int event)
{
    {
        std::unique_lock<std::mutex> lock(cvMtx);
        triggered = true;
        cv.notify_one();
    }

    OnTrigger(event);
}

bool Event::StartWait(EventWaiter waiter, EventTrigger * trigger)
{
    std::unique_lock<std::mutex> lock(mtx_);
    if (closeTrigger_) return false;

    if ((*waiter.events & POLLIN) && Readable())
        *waiter.revents |= POLLIN;
    if ((*waiter.events & POLLOUT) && Writable())
        *waiter.revents |= POLLOUT;
    if (Error())
        *waiter.revents |= POLLERR;

    waitings_[trigger] = waiter;
    return true;
}

void Event::Trigger(short int event)
{
    std::unique_lock<std::mutex> lock(mtx_);
    TriggerWithoutLock(event);
}

void Event::TriggerWithoutLock(short int event)
{
    for (auto & kv : waitings_) {
        EventTrigger * trigger = kv.first;
        EventWaiter & waiter = kv.second;
        switch (event) {
            case POLLIN:
                if (*waiter.events & POLLIN) {
                    DebugPrint(dbg_event, "fd = %d, epfd = %d, trigger event = POLLIN. waiter.revents = %s",
                            Fd(), trigger->epollfd, PollEvent2Str(*waiter.revents));
                    __atomic_fetch_or(waiter.revents, POLLIN, std::memory_order_seq_cst);
                    trigger->Trigger(POLLIN);
                }
                break;

            case POLLOUT:
                if (*waiter.events & POLLOUT) {
                    DebugPrint(dbg_event, "fd = %d, epfd = %d, trigger event = POLLOUT. waiter.revents = %s",
                            Fd(), trigger->epollfd, PollEvent2Str(*waiter.revents));
                    __atomic_fetch_or(waiter.revents, POLLOUT, std::memory_order_seq_cst);
                    trigger->Trigger(POLLOUT);
                }
                break;

            case POLLERR:
                DebugPrint(dbg_event, "fd = %d, epfd = %d, trigger event = POLLERR. waiter.revents = %s",
                        Fd(), trigger->epollfd, PollEvent2Str(*waiter.revents));
                __atomic_fetch_or(waiter.revents, POLLERR, std::memory_order_seq_cst);
                trigger->Trigger(POLLERR);
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
    DebugPrint(dbg_event, "fd = %d, SetReadable(%s)", Fd(), b ? "true" : "false");
    readable = b;
    Trigger(POLLIN);
}
void Event::SetWritable(bool b)
{
    DebugPrint(dbg_event, "fd = %d, SetWritable(%s)", Fd(), b ? "true" : "false");
    writable = b;
    Trigger(POLLOUT);
}
void Event::SetError(int err, int quicErr)
{
    DebugPrint(dbg_event, "fd = %d, SetError(err=%d, quicErr=%d) now-error=%d",
            Fd(), err, quicErr, error);
    if (error == 0) {
        error = err;
        quicErrorCode = (QuicErrorCode)quicErr;
        Trigger(POLLERR);
    }
}

void Event::ClearWaitingsByClose()
{
    if (closeTrigger_) return ;

    std::unique_lock<std::mutex> lock(mtx_);
    if (closeTrigger_) return ;

    std::vector<EventTrigger*> closed;
    closeTrigger_ = true;
    for (auto & kv : waitings_) {
        closed.push_back(kv.first);
    }
    waitings_.clear();
    lock.unlock();

    for (EventTrigger* trigger : closed) {
        trigger->OnClose(this);
    }
}

std::string Event::GetDebugInfo(int indent)
{
    std::string idt(indent, ' ');
    std::string idt2 = idt + ' ';
    std::string idt3 = idt2 + ' ';

    std::string info;
    std::string event;
    if (Readable()) event += "readable |";
    if (Writable()) event += "writable |";
    if (Error()) event += Format("error = %d |", error);
    if (event.empty()) event = "nil";
    else event.resize(event.size() - 2);

    info += idt + P("Event: %s", event.c_str());
    std::unique_lock<std::mutex> lock(mtx_);
    info += idt + P("Listener: %d", (int)waitings_.size());
    for (auto & kv : waitings_) {
        int epfd = kv.first->epollfd;
        info += idt2 + P("-> epfd = %d, events = %s, revents = %s",
                epfd, PollEvent2Str(*kv.second.events), PollEvent2Str(*kv.second.revents));
    }

    return info;
}

} // namespace posix_quic
