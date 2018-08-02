#include "header_parser.h"
#include "clock.h"

namespace posix_quic {

HeaderParser::HeaderParser()
    : serverFramer_(
            net::AllSupportedVersions(),
            net::QuicTime::Zero(),
            net::Perspective::IS_SERVER
            ),
    clientFramer_(
            net::AllSupportedVersions(),
            net::QuicTime::Zero(),
            net::Perspective::IS_SERVER
            ),
    connectionId_(INVALID_QUIC_CONNECTION_ID)
{
    serverFramer_.set_visitor(this);
    clientFramer_.set_visitor(this);
}

QuicConnectionId HeaderParser::ParseHeader(const char* data, size_t len)
{
    std::unique_lock<std::mutex> lock(mtx_);
    data_ = data;
    len_ = len;
    connectionId_ = INVALID_QUIC_CONNECTION_ID;
    serverFramer_.ProcessPacket(QuicEncryptedPacket(data_, len_));
    return connectionId_;
}

bool HeaderParser::OnUnauthenticatedPublicHeader(const QuicPacketHeader& header)
{
    connectionId_ = header.connection_id;
    return false;
}

void HeaderParser::OnError(QuicFramer* framer)
{
    if (framer == &serverFramer_) {
        clientFramer_.ProcessPacket(QuicEncryptedPacket(data_, len_));
    }
}

} // namespace posix_quic
