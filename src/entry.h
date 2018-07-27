#pragma once

#include "net/quic/quartc/quartc_session.h"
#include "net/quic/quartc/quartc_stream_interface.h"
#include "event.h"

namespace posix_quic {

// uint64_t
using net::QuicConnectionId;
using net::QuicStreamId;

struct EntryBase : public Event
{
};

typedef std::shared_ptr<EntryBase> EntryPtr;
typedef std::weak_ptr<EntryBase> EntryWeakPtr;

} // namespace posix_quic
