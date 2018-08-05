#pragma once

#include "fwd.h"
#include <memory>
#include <mutex>
#include <condition_variable>
#include <set>
#include "fd.h"

namespace posix_quic {

class Event : public FdBase
{
public:
    virtual ~Event() {}

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

    bool StartWait(EventWaiter waiter, EventTrigger * trigger);
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

    std::string GetDebugInfo(int indent);

protected:
    void TriggerWithoutLock(int event);

    void ClearWaitingsByClose();

private:
    bool readable = false;
    bool writable = false;
    bool closeByPeer = false;
    QuicErrorCode quicErrorCode = net::QUIC_NO_ERROR;
    int error = 0;

    std::mutex mtx_;
    std::map<EventTrigger*, EventWaiter> waitings_;
    bool closeTrigger_ = false;
};

} // namespace posix_quic
