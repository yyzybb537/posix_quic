#pragma once

#include "net/quic/quartc/quartc_session.h"

namespace posix_quic {

class PosixQuicSession
{
public:
    using QuartcSession::QuartcSession;

    // 创建自定义的Stream, OnDataAvailible时不再立即读取数据.
    QuicStream* CreateIncomingDynamicStream(QuicStreamId id) override;

    QuicStream* CreateOutgoingDynamicStream() override;

    void OnConnectionClosed(QuicErrorCode error, const std::string& error_details,
            ConnectionCloseSource source) override {
        QuartcSession::OnConnectionClosed(error, error_details, source);
    }

protected:
    // Called when the crypto handshake is complete.
    virtual void OnCryptoHandshakeComplete() {}

    // Called when a new stream is received from the remote endpoint.
    virtual void OnIncomingStream(QuartcStreamInterface* stream) {}

    // Called when the connection is closed. This means all of the streams will
    // be closed and no new streams can be created.
    virtual void OnConnectionClosed(int error_code, bool from_remote) {}
};

} // namespace posix_quic
