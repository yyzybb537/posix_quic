#pragma once

#include "net/quic/quartc/quartc_session.h"
#include "net/quic/quartc/quartc_stream_interface.h"
#include "event.h"

namespace posix_quic {

// uint64_t
using net::QuicConnectionId;
using net::QuicStreamId;

class EntryBase;
typedef std::shared_ptr<EntryBase> EntryPtr;
typedef std::weak_ptr<EntryBase> EntryWeakPtr;

enum class EntryCategory : int8_t {
    Socket,
    Stream,
};

class EntryBase : public Event
{
public:
    void SetFd(int fd) { fd_ = fd; }
    int Fd() const { return fd_; }

    virtual EntryCategory Category() const = 0;

protected:
    static FdFactory & GetFdFactory();
    static FdManager<EntryPtr> & GetFdManager();

private:
    int fd_ = 0;
};

} // namespace posix_quic
