#pragma once

#include <memory>

namespace posix_quic {

class QuicSocketEntry;
typedef std::weak_ptr<QuicSocketEntry> QuicSocketEntryWeakPtr;
typedef std::shared_ptr<QuicSocketEntry> QuicSocketEntryPtr;

class QuicStreamEntry;
typedef std::weak_ptr<QuicStreamEntry> QuicStreamEntryWeakPtr;
typedef std::shared_ptr<QuicStreamEntry> QuicStreamEntryPtr;

} // namespace posix_quic
