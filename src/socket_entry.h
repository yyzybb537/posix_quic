#pragma once

#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include "fwd.h"
#include "entry.h"
#include "session.h"
#include "packet_transport.h"

namespace posix_quic {

// non-blocking quic fd.
class QuicSocketEntry
    : public EntryBase,
    public std::enable_shared_from_this<QuicSocketEntry>,
    public net::QuartcSession,
    private net::QuicSessionInterface::Delegate
{
public:
    using QuartcSession::QuartcSession;

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

    int Close();

    QuicSocketEntryPtr AcceptSocket();

    QuicStreamEntryPtr AcceptStream();

    // create outgoing stream
    QuicStreamEntryPtr CreateStream();

    void CloseStream(uint64_t streamId);

    // --------------------------------
    // client和server建立连接都需要Handshake, 都需要判断这个.
    bool IsConnected();

    // --------------------------------
    int NativeUdpFd() const;

    // --------------------------------
    // Called in epoll trigger loop
    void OnCanWrite() override;

    void ProcessUdpPacket(const QuicSocketAddress& self_address,
            const QuicSocketAddress& peer_address,
            const QuicReceivedPacket& packet) override;
    // --------------------------------

    QuartcStreamPtr GetQuartcStream(QuicStreamId streamId);

    static QuartcFactory& GetQuartcFactory();
    static QuicSocketEntryPtr NewQuicSocketEntry();
    static void DeleteQuicSocketEntry(QuicSocketEntryPtr ptr);

private:
    static FdFactory & GetFdFactory();
    static FdManager<QuicSocketEntryPtr> & GetFdManager();

    // -----------------------------------------------------------------
    // QuicSessionInterface::Delegate
private:
    void OnCryptoHandshakeComplete() override;

    void OnIncomingStream(QuartcStreamInterface* stream) override;

    void OnConnectionClosed(int error_code, bool from_remote) override;
    // -----------------------------------------------------------------

private:
    void OnAccept(std::shared_ptr<int> udpSocket, const struct sockaddr* addr,
            socklen_t addrlen);

    int CreateNewUdpSocket();

private:
    std::mutex mtx_;

    std::shared_ptr<int> udpSocket_;
    QuicSocketState socketState_ = QuicSocketState_None;

    // accept socket queue
    std::mutex acceptQueueMtx_;
    std::queue<QuicSocketEntryPtr> acceptQueue_;
    std::set<QuicSocketEntryPtr> synQueue_;

    // accept stream queue
    std::mutex streamQueueMtx_;
    std::queue<QuicSocketEntryPtr> streamQueue_;

    std::shared_ptr<PacketTransport> packetTransport_;
};

} // namespace posix_quic
