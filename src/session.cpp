#include "session.h"
#include "stream_entry.h"

namespace posix_quic {

QuicStream* PosixQuicSession::CreateIncomingDynamicStream(QuicStreamId id) override
{
    std::unique_ptr<QuicStream> ptr(new QuicStreamEntry(id, this));
    QuicStream* raw = ptr.release();
    ActivateStream(ptr);
    return raw;
}

QuicStream* PosixQuicSession::CreateOutgoingDynamicStream() override
{
    std::unique_ptr<QuicStream> ptr(new QuicStreamEntry(GetNextOutgoingStreamId(), this));
    QuicStream* raw = ptr.release();
    ActivateStream(ptr);
    return raw;
}

} // namespace posix_quic
