#pragma once

#include <memory>

namespace posix_quic {

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
