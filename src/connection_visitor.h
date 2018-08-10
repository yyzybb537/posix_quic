#pragma once

#include "fwd.h"
#include "net/quic/core/quic_connection.h"
#include "fd.h"
#include "packet_transport.h"

namespace posix_quic {

class QuicConnectionVisitor
    : public net::QuicConnectionDebugVisitor
{
public:
    explicit QuicConnectionVisitor();
    virtual ~QuicConnectionVisitor();

    void Bind(std::mutex * mtx, QuicConnection * connection,
            QuicSocketOptions * opts, FdBase * parent,
            std::shared_ptr<PosixQuicPacketTransport> packetTransport);

    std::mutex & GetAlarmLock() { return *mtx_; }

    void CheckForNoAckTimeout();

    void SetNoAckAlarm();

    void CancelNoAckAlarm();

    // Called when a packet has been sent.
    void OnPacketSent(const SerializedPacket& serialized_packet,
            QuicPacketNumber original_packet_number,
            TransmissionType transmission_type,
            QuicTime sent_time) override;

    // Called when a PING frame has been sent.
    void OnPingSent() override;

    // Called when a AckFrame has been parsed.
    void OnAckFrame(const QuicAckFrame& frame) override;

private:
    // Called when a packet has been received, but before it is
    // validated or parsed.
    void OnPacketReceived(const QuicSocketAddress& self_address,
            const QuicSocketAddress& peer_address,
            const QuicEncryptedPacket& packet) override;

    // Called when the unauthenticated portion of the header has been parsed.
    void OnUnauthenticatedHeader(const QuicPacketHeader& header) override;

    // Called when a packet is received with a connection id that does not
    // match the ID of this connection.
    void OnIncorrectConnectionId(QuicConnectionId connection_id) override;

    // Called when an undecryptable packet has been received.
    void OnUndecryptablePacket() override;

    // Called when a duplicate packet has been received.
    void OnDuplicatePacket(QuicPacketNumber packet_number) override;

    // Called when the protocol version on the received packet doensn't match
    // current protocol version of the connection.
    void OnProtocolVersionMismatch(ParsedQuicVersion version) override;

    // Called when the complete header of a packet has been parsed.
    void OnPacketHeader(const QuicPacketHeader& header) override;

    // Called when a StreamFrame has been parsed.
    void OnStreamFrame(const QuicStreamFrame& frame) override;

    // Called when a StopWaitingFrame has been parsed.
    void OnStopWaitingFrame(const QuicStopWaitingFrame& frame) override;

    // Called when a QuicPaddingFrame has been parsed.
    void OnPaddingFrame(const QuicPaddingFrame& frame) override;

    // Called when a Ping has been parsed.
    void OnPingFrame(const QuicPingFrame& frame) override;

    // Called when a GoAway has been parsed.
    void OnGoAwayFrame(const QuicGoAwayFrame& frame) override;

    // Called when a RstStreamFrame has been parsed.
    void OnRstStreamFrame(const QuicRstStreamFrame& frame) override;

    // Called when a ConnectionCloseFrame has been parsed.
    void OnConnectionCloseFrame(const QuicConnectionCloseFrame& frame) override;

    // Called when a WindowUpdate has been parsed.
    void OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame,
            const QuicTime& receive_time) override;

    // Called when a BlockedFrame has been parsed.
    void OnBlockedFrame(const QuicBlockedFrame& frame) override;

    // Called when a public reset packet has been received.
    void OnPublicResetPacket(const QuicPublicResetPacket& packet) override;

    // Called when a version negotiation packet has been received.
    void OnVersionNegotiationPacket(
            const QuicVersionNegotiationPacket& packet) override;

    // Called when the connection is closed.
    void OnConnectionClosed(QuicErrorCode error,
            const QuicString& error_details,
            ConnectionCloseSource source) override;

    // Called when the version negotiation is successful.
    void OnSuccessfulVersionNegotiation(
            const ParsedQuicVersion& version) override;

    // Called when a CachedNetworkParameters is sent to the client.
    void OnSendConnectionState(
            const CachedNetworkParameters& cached_network_params) override;

    // Called when a CachedNetworkParameters are received from the client.
    void OnReceiveConnectionState(
            const CachedNetworkParameters& cached_network_params) override;

    // Called when the connection parameters are set from the supplied
    // |config|.
    void OnSetFromConfig(const QuicConfig& config) override;

    // Called when RTT may have changed, including when an RTT is read from
    // the config.
    void OnRttChanged(QuicTime::Delta rtt) const override;

private:
    std::mutex * mtx_ = nullptr;

    QuicConnection* connection_ = nullptr;

    QuicSocketOptions * opts_ = nullptr;

    FdBase * parent_ = nullptr;

    std::shared_ptr<PosixQuicPacketTransport> packetTransport_;

    std::unique_ptr<QuicAlarm> noAckAlarm_;

    volatile bool canceled_ = false;

    QuicTime lastAckTime_;
    QuicTime lastSendTime_;
};

} // namespace posix_quic

