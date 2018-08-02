#pragma once

#include "fwd.h"
#include <memory>
#include <mutex>
#include <condition_variable>
#include <set>

namespace posix_quic {

class Event
{
public:
    virtual ~Event() {}

    void SetFd(int fd) { fd_ = fd; }
    int Fd() const { return fd_; }

    struct EventWaiter {
        short int* events;
        short int* revents;
    };

    struct EventTrigger {
        std::condition_variable cv;
        std::mutex cvMtx;
        volatile bool triggered = false;
        int epollfd = -1;

        void Wait(int timeout);
        void Trigger();
    };

    int StartWait(EventWaiter waiter, EventTrigger * trigger);
    void StopWait(EventTrigger * trigger);
    void Trigger(int event);

    void SetReadable(bool b);
    void SetWritable(bool b);
    void SetError(int err, int quicErr = 0);
    void SetCloseByPeer(bool b) { closeByPeer = b; }

    bool Readable() { return readable; }
    bool Writable() { return writable; }
    int Error() { return error; }
    bool IsCloseByPeer() { return closeByPeer; }
    QuicErrorCode GetQuicErrorCode() { return quicErrorCode; }

    virtual const char* DebugTypeInfo() { return ""; };

private:
    bool readable = false;
    bool writable = false;
    bool closeByPeer = false;
    QuicErrorCode quicErrorCode = net::QUIC_NO_ERROR;
    int error = 0;

    int fd_ = 0;

    std::mutex mtx_;
    std::map<EventTrigger*, EventWaiter> waitings_;
};

} // namespace posix_quic
