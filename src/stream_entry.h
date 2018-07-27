#pragma once

#include "entry.h"
#include "stream.h"
#include <memory>

namespace posix_quic {

class QuicStreamEntry;
typedef std::weak_ptr<QuicStreamEntry> QuicStreamEntryWeakPtr;
typedef std::shared_ptr<QuicStreamEntry> QuicStreamEntryPtr;

class QuicStreamEntry : public EntryBase, public PosixQuicStream
{
public:
    ssize_t Write(const void* data, size_t length, bool fin = false);

    ssize_t Read(const void* data, size_t length);

    int Close();

private:
};

} // namespace posix_quic
