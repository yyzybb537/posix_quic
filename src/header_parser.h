#pragma once

#include "fwd.h"
#include "net/quic/core/quic_framer.h"
#include <mutex>

namespace posix_quic {

class HeaderParser : private QuicFramerVisitorInterface
{
public:
    HeaderParser();

    QuicConnectionId ParseHeader(const char* data, size_t len);

    // Called when all fields except packet number has been parsed, but has not
    // been authenticated. If it returns false, framing for this packet will
    // cease.
    bool OnUnauthenticatedPublicHeader(const QuicPacketHeader& header) override;

    // Called if an error is detected in the QUIC protocol.
    void OnError(QuicFramer* framer) override;

private:
    std::mutex mtx_;

    QuicFramer serverFramer_;
    QuicFramer clientFramer_;

    QuicConnectionId connectionId_;

    const char* data_;
    size_t len_;

private:
    // Called only when |perspective_| is IS_SERVER and the framer gets a
    // packet with version flag true and the version on the packet doesn't match
    // |quic_version_|. The visitor should return true after it updates the
    // version of the |framer_| to |received_version| or false to stop processing
    // this packet.
    bool OnProtocolVersionMismatch(
            net::ParsedQuicVersion received_version) override { return true; }

    // Called when a new packet has been received, before it
    // has been validated or processed.
    void OnPacket() override {}

    // Called when a public reset packet has been parsed but has not yet
    // been validated.
    void OnPublicResetPacket(const net::QuicPublicResetPacket& packet) override {}

    // Called only when |perspective_| is IS_CLIENT and a version negotiation
    // packet has been parsed.
    void OnVersionNegotiationPacket(
            const net::QuicVersionNegotiationPacket& packet) override {}

    // Called when the unauthenticated portion of the header has been parsed.
    // If OnUnauthenticatedHeader returns false, framing for this packet will
    // cease.
    bool OnUnauthenticatedHeader(const net::QuicPacketHeader& header) override { return true; }

    // Called when a packet has been decrypted. |level| is the encryption level
    // of the packet.
    void OnDecryptedPacket(net::EncryptionLevel level) override {}

    // Called when the complete header of a packet had been parsed.
    // If OnPacketHeader returns false, framing for this packet will cease.
    bool OnPacketHeader(const net::QuicPacketHeader& header) override { return true; }

    // Called when a StreamFrame has been parsed.
    bool OnStreamFrame(const net::QuicStreamFrame& frame) override { return true; }

    // Called when a AckFrame has been parsed.  If OnAckFrame returns false,
    // the framer will stop parsing the current packet.
    bool OnAckFrame(const net::QuicAckFrame& frame) override { return true; }

    // Called when largest acked of an AckFrame has been parsed.
    bool OnAckFrameStart(net::QuicPacketNumber largest_acked,
            net::QuicTime::Delta ack_delay_time) override { return true; }

    // Called when ack range [start, end) of an AckFrame has been parsed.
    bool OnAckRange(net::QuicPacketNumber start,
            net::QuicPacketNumber end,
            bool last_range) override { return true; }

    // Called when a StopWaitingFrame has been parsed.
    bool OnStopWaitingFrame(const net::QuicStopWaitingFrame& frame) override { return true; }

    // Called when a QuicPaddingFrame has been parsed.
    bool OnPaddingFrame(const net::QuicPaddingFrame& frame) override { return true; }

    // Called when a PingFrame has been parsed.
    bool OnPingFrame(const net::QuicPingFrame& frame) override { return true; }

    // Called when a RstStreamFrame has been parsed.
    bool OnRstStreamFrame(const net::QuicRstStreamFrame& frame) override { return true; }

    // Called when a ConnectionCloseFrame has been parsed.
    bool OnConnectionCloseFrame(
            const net::QuicConnectionCloseFrame& frame) override { return true; }

    // Called when a GoAwayFrame has been parsed.
    bool OnGoAwayFrame(const net::QuicGoAwayFrame& frame) override { return true; }

    // Called when a WindowUpdateFrame has been parsed.
    bool OnWindowUpdateFrame(const net::QuicWindowUpdateFrame& frame) override { return true; }

    // Called when a BlockedFrame has been parsed.
    bool OnBlockedFrame(const net::QuicBlockedFrame& frame) override { return true; }

    // Called when a packet has been completely processed.
    void OnPacketComplete() override {}

    // Called to check whether |token| is a valid stateless reset token.
    bool IsValidStatelessResetToken(net::uint128 token) const override { return true; }

    // Called when an IETF stateless reset packet has been parsed and validated
    // with the stateless reset token.
    void OnAuthenticatedIetfStatelessResetPacket(
            const net::QuicIetfStatelessResetPacket& packet) override {}

};

} // namespace posix_quic
