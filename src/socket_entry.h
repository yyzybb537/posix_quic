#pragma once

#include "fwd.h"
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <list>
#include "entry.h"
#include "packet_transport.h"
#include "connection_manager.h"
#include "net/quic/quartc/quartc_session_interface.h"

namespace posix_quic {

// non-blocking quic fd.
class QuicSocketEntry
    : public EntryBase,
    public std::enable_shared_from_this<QuicSocketEntry>,
    public QuartcSession,
    private QuartcSessionInterface::Delegate
{
public:
    using QuartcSession::QuartcSession;

    EntryCategory Category() const override { return EntryCategory::Socket; }

    // 这个enum糅合了udp和quic两层的状态
    enum QuicSocketState {
        // 没有绑定udp socket
        QuicSocketState_None,

        // 绑定了一个共享的udp socket(不能bind)
        QuicSocketState_Shared,

        // 初始化了一个udp socket
        QuicSocketState_Inited,

        // 初始化了一个udp socket并bind了一个端口
        QuicSocketState_Binded,

        // Quic连接中
        QuicSocketState_Connecting,

        // Quic已连接
        QuicSocketState_Connected,

        // 已关闭
        QuicSocketState_Closed,
    };

    int Bind(const struct sockaddr* addr, socklen_t addrlen);

    int Connect(const struct sockaddr* addr, socklen_t addrlen);

    int Close() override;

    QuicSocketEntryPtr AcceptSocket();

    QuicStreamEntryPtr AcceptStream();

    // create outgoing stream
    QuicStreamEntryPtr CreateStream();

    void CloseStream(uint64_t streamId);

    // --------------------------------
    // client和server建立连接都需要Handshake, 都需要判断这个.
    bool IsConnected();

    // --------------------------------
    std::shared_ptr<int> NativeUdpFd() const override;

    // --------------------------------
    // Called in epoll trigger loop
    void ProcessUdpPacket(const QuicSocketAddress& self_address,
            const QuicSocketAddress& peer_address,
            const QuicReceivedPacket& packet) override;
    // --------------------------------

    QuartcStreamPtr GetQuartcStream(QuicStreamId streamId);

    static QuartcFactory& GetQuartcFactory();
    static QuicSocketEntryPtr NewQuicSocketEntry(bool isServer, QuicConnectionId id = INVALID_QUIC_CONNECTION_ID);
    static void DeleteQuicSocketEntry(QuicSocketEntryPtr ptr);

    static ConnectionManager & GetConnectionManager();

    void OnSyn(QuicSocketEntryPtr owner, QuicSocketAddress address);

    std::string GetDebugInfo(int indent);

    // -----------------------------------------------------------------
    // QuartcSessionInterface::Delegate
private:
    void OnCryptoHandshakeComplete() override;

    void OnIncomingStream(QuartcStreamInterface* stream) override;

    void OnConnectionClosed(int error_code, bool from_remote) override;
    // -----------------------------------------------------------------

private:
    void PushAcceptQueue(QuicSocketEntryPtr entry);

    int CreateNewUdpSocket();

    void ClearAcceptSocketByClose();

private:
    std::mutex mtx_;

    std::shared_ptr<int> udpSocket_;
    QuicSocketState socketState_ = QuicSocketState_None;

    // accept socket queue
    std::mutex acceptSocketsMtx_;
    std::list<QuicSocketEntryPtr> acceptSockets_;

    // accept stream queue
    std::mutex acceptStreamsMtx_;
    std::list<QuicStreamEntryPtr> acceptStreams_;

    std::shared_ptr<PosixQuicPacketTransport> packetTransport_;
};

} // namespace posix_quic
