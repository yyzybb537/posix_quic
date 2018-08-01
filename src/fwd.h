#pragma once

#include <memory>
#include "net/quic/quartc/quartc_session.h"

namespace posix_quic {

// uint64_t
using net::QuicConnectionId;

// uint32_t
using net::QuicStreamId;

#define INVALID_QUIC_CONNECTION_ID QuicConnectionId(-1)

typedef int UdpSocket;
typedef int QuicSocket;
typedef int QuicStream;

class QuicSocketEntry;
typedef std::weak_ptr<QuicSocketEntry> QuicSocketEntryWeakPtr;
typedef std::shared_ptr<QuicSocketEntry> QuicSocketEntryPtr;

class QuicStreamEntry;
typedef std::weak_ptr<QuicStreamEntry> QuicStreamEntryWeakPtr;
typedef std::shared_ptr<QuicStreamEntry> QuicStreamEntryPtr;

class QuicEpollerEntry;
typedef std::weak_ptr<QuicEpollerEntry> QuicEpollerEntryWeakPtr;
typedef std::shared_ptr<QuicEpollerEntry> QuicEpollerEntryPtr;

} // namespace posix_quic
