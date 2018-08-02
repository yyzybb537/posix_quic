#pragma once

#include "fwd.h"
#include "net/quic/quartc/quartc_session.h"
#include "net/quic/quartc/quartc_stream_interface.h"
#include "event.h"
#include "fd_factory.h"
#include "fd_manager.h"

namespace posix_quic {

class EntryBase;
typedef std::shared_ptr<EntryBase> EntryPtr;
typedef std::weak_ptr<EntryBase> EntryWeakPtr;

enum class EntryCategory : int8_t {
    Invalid,
    Socket,
    Stream,
};

class EntryBase : public Event
{
public:
    virtual EntryCategory Category() const = 0;

    virtual std::shared_ptr<int> NativeUdpFd() const = 0;

    virtual int Close() = 0;

    static FdFactory & GetFdFactory();

    static FdManager<EntryPtr> & GetFdManager();

    const char* DebugTypeInfo() override { return EntryCategory2Str((int)Category()); };
};

} // namespace posix_quic
