#pragma once

#include <memory>
#include "net/quic/quartc/quartc_session_interface.h"
#include "net/quic/quartc/quartc_session.h"
#include "net/quic/quartc/quartc_stream_interface.h"

class QuicSessionImpl;
typedef std::shared_ptr<QuicSessionImpl> QuicSessionImplPtr;

// uint64_t
using net::QuicConnectionId;
using net::QuicStreamId;

// non-blocking quic fd.
class QuicSessionImpl 
{
public:
    QuicSessionImpl();
    ~QuicSessionImpl();

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
    };

    int Bind(const struct sockaddr* addr, socklen_t addrlen);

    int Connect(const struct sockaddr* addr, socklen_t addrlen);

    void Close();

    QuicSessionImplPtr Accept();

    enum StreamIdCategory {
        // 使用一个公用的Stream
        eStreamId_Shared = 0,

        // 使用上一次使用的Stream
        eStreamId_Last = -1,

        // 总是申请新的stream
        eStreamId_AlwaysNew = -2,
    };

    std::pair<ssize_t, uint64_t> Write(const void* data, int length,
            QuicStreamId streamId = eStreamId_Shared, bool fin = false);

    std::pair<ssize_t, uint64_t> Read(const void* data, int length,
            QuicStreamId streamId = eStreamId_Shared);

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

    void OnError();
    // --------------------------------

private:
    void OnAccept(int fd, const char* host, int port);

    int CreateNewUdpSocket();

private:
    std::unique_ptr<net::QuartcSession> session_;
    std::shared_ptr<int> udpSocket_;
    uint64_t sharedStreamId = 0;
    uint64_t lastStreamId = 0;
    QuicSocketState socketState_ = QuicSocketState_None;
};

