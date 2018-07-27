#include "session.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>

QuicSessionImpl::QuicSessionImpl()
{

}
QuicSessionImpl::~QuicSessionImpl()
{

}
int QuicSessionImpl::CreateNewUdpSocket()
{
    if (udpSocket_) {
        errno = EINVAL;
        return -1;
    }

    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) return -1;

    udpSocket_.reset(new int(fd), [](int* p){
                ::close(*p);
                delete p;
            });
    socketState_ = QuicSocketState_Inited;
    return 0;
}

int QuicSessionImpl::Bind(const struct sockaddr* addr, socklen_t addrlen)
{
    switch (socketState_) {
        case QuicSocketState_None:
            int res = CreateNewUdpSocket();
            if (res < 0) return -1;

        case QuicSocketState_Inited:
            return ::bind(*udpSocket_, addr, addrlen);

        default:
            errno = EINVAL;
            return -1;
    }
}
int QuicSessionImpl::Connect(const struct sockaddr* addr, socklen_t addrlen)
{
    switch (socketState_) {
        case QuicSocketState_None:
            int res = CreateNewUdpSocket();
            if (res < 0) return -1;
            break;

        case QuicSocketState_Inited:
        case QuicSocketState_Binded:
            break;

        case QuicSocketState_Connecting:
            errno = EISCONN;
            return -1;

        default:
            errno = EINVAL;
            return -1;
    }

    if (session_) session_.reset();

    QuartcFactoryInterface::QuartcSessionConfig sessionConfig;
    sessionConfig.is_server = false;
    sessionConfig.unique_remote_server_id = serverIp;
    sessionConfig.packet_transport = mPacketTransport.get();
    sessionConfig.connection_id = QuicRandom::GetInstance()->RandUint64();
    sessionConfig.congestion_control = QuartcCongestionControl::kDefault;
}
void QuicSessionImpl::Close()
{

}
int QuicSessionImpl::Accept()
{

}
int QuicSessionImpl::Write()
{

}
int QuicSessionImpl::Read()
{

}
void QuicSessionImpl::CloseStream(int streamId)
{

}
bool QuicSessionImpl::IsConnected()
{

}
int QuicSessionImpl::NativeUdpFd()const 
{

}
int QuicSessionImpl::GetConnectionId()const 
{

}
void QuicSessionImpl::OnCanWrite()
{

}
void QuicSessionImpl::OnCanRead()
{

}
void QuicSessionImpl::OnError()
{

}
void QuicSessionImpl::OnAccept(int fd, const char * host, int port)
{

}

