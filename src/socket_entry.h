#pragma once

#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include "entry.h"
#include "session.h"

namespace posix_quic {

class QuicSocketEntry;
typedef std::weak_ptr<QuicSocketEntry> QuicSocketEntryWeakPtr;
typedef std::shared_ptr<QuicSocketEntry> QuicSocketEntryPtr;

// non-blocking quic fd.
class QuicSocketEntry
    : public EntryBase,
    public net::QuartcSession,
    private net::QuicSessionInterface::Delegate
{
public:
    QuicSocketEntry(QuicConnectionId id);

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

    void CloseStream(uint64_t streamId);

    // --------------------------------
    // client和server建立连接都需要Handshake, 都需要判断这个.
    bool IsConnected();

    // --------------------------------
    int NativeUdpFd() const;

    QuicConnectionId GetConnectionId() const;

    // --------------------------------
    // Called in epoll trigger loop
    void OnCanWrite();

    void OnCanRead();
    // --------------------------------

private:
    void OnAccept(int fd, const char* host, int port);

    int CreateNewUdpSocket();

private:
    std::shared_ptr<int> udpSocket_;
    QuicConnectionId connectionId_ = 0;
    QuicSocketState socketState_ = QuicSocketState_None;

    // accept queue
    std::mutex acceptQueueMtx_;
    std::queue<QuicSocketEntryPtr> acceptQueue_;
    std::set<QuicSocketEntryPtr> synQueue_;
};

} // namespace posix_quic
