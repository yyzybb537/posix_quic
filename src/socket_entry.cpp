#include "socket_entry.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>

namespace posix_quic {

using namespace net;

QuicSocketEntry::QuicSocketEntry(QuicConnectionId id)
    : QuartcSession(id)
{
    SetDelegate(this);
}

int QuicSocketEntry::CreateNewUdpSocket()
{
    if (udpSocket_) {
        errno = EINVAL;
        return -1;
    }

    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) return -1;

    int val = ::fcntl(fd, F_GETFL, 0);
    if (val == -1) {
        ::close(fd);
        return -1;
    }
    int res = ::fcntl(fd, F_SETFL, val | O_NONBLOCK);
    if (res == -1) {
        ::close(fd);
        return -1;
    }

    udpSocket_.reset(new int(fd), [](int* p){
                ::close(*p);
                delete p;
            });
    socketState_ = QuicSocketState_Inited;
    return 0;
}

int QuicSocketEntry::Bind(const struct sockaddr* addr, socklen_t addrlen)
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
int QuicSocketEntry::Connect(const struct sockaddr* addr, socklen_t addrlen)
{
    switch (socketState_) {
        case QuicSocketState_None:
            if (CreateNewUdpSocket() < 0) return -1;
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

    socketState_ = QuicSocketState_Connecting;

    QuartcFactoryInterface::QuartcSessionConfig sessionConfig;
    sessionConfig.is_server = false;
    sessionConfig.unique_remote_server_id = serverIp;
    sessionConfig.packet_transport = mPacketTransport.get();
    sessionConfig.connection_id = connectionId_;
    sessionConfig.congestion_control = QuartcCongestionControl::kDefault;
    // TODO: connect
}
int QuicSocketEntry::Close()
{
    socketState_ = QuicSocketState_Closed;
    CloseConnection("");
    return 0;
}
QuicSocketEntryPtr QuicSocketEntry::Accept()
{
    if (socketState_ != QuicSocketState_Binded) {
        errno = EINVAL;
        return QuicSocketEntryPtr();
    }

    std::unique_lock<std::mutex> lock(acceptQueueMtx_);
    if (acceptQueue_.empty()) {
        errno = EAGAIN;
        return QuicSocketEntryPtr();
    }

    auto ptr = acceptQueue_.front();
    acceptQueue_.pop();
    return ptr;
}
std::pair<ssize_t, QuicStreamId> QuicSocketEntry::Write(const void* data,
        size_t length, QuicStreamId streamId, bool fin)
{
    if (socketState_ != QuicSocketState_Connected) {
        errno = EINVAL;
        return -1;
    }

    QuartcStream* outputStream = nullptr;

    switch (streamId) {
        case eStreamId_Shared:
            if (!sharedStreamId_) {
                outputStream = CreateOutgoingStream(QuartcSessionInterface::OutgoingStreamParameters{});
                if (outputStream)
                    sharedStreamId_ = outputStream->stream_id();
            }
            break;

        case eStreamId_Last:
            if (lastStreamId_ && session_->IsOpenStream(lastStreamId_)) {
                outputStream = (QuartcStream*)session_->GetOrCreateStream(lastStreamId_);
                break;
            }

        case eStreamId_AlwaysNew:
            outputStream = session_->CreateOutgoingStream(QuartcSessionInterface::OutgoingStreamParameters{});
            break;

        default:
            outputStream = (QuartcStream*)session_->GetOrCreateStream(streamId);
            break;
    }

    if (!outputStream) {
        errno = ENOMEM;
        return -1;
    }

    scoped_refptr<IOBuffer> buffer(new WrappedIOBuffer(data));
    QuicMemSliceSpanImpl memSliceSpanImpl(&buffer, &length, 1);
    QuicMemSliceSpan memSliceSpan(memSliceSpanImpl);
    QuartcStreamInterface::WriteParameters writeParameters;
    if (!outputStream->Write(memSliceSpan, writeParameters)) {
        errno = EAGAIN;
        return -1;
    }

    return length;
}
std::pair<ssize_t, QuicStreamId> QuicSocketEntry::Read(const void* data,
        size_t length, QuicStreamId streamId)
{
    if (socketState_ != QuicSocketState_Connected) {
        errno = EINVAL;
        return -1;
    }

    assert(!!session_);

    QuartcStream* outputStream = (QuartcStream*)session_->GetOrCreateStream(streamId);

    if (outputStream->ReadableBytes() == 0) {
        errno = EAGAIN;
        return -1;
    }
}
void QuicSocketEntry::CloseStream(int streamId)
{
    if (session_)
        session_->CloseStream(streamId);
}
bool QuicSocketEntry::IsConnected()
{
    return socketState_ == QuicSocketState_Connected;
}
int QuicSocketEntry::NativeUdpFd()const 
{
    return udpSocket_ ? *udpSocket_ : -1;
}
int QuicSocketEntry::GetConnectionId()const 
{
    return connectionId_;
}
void QuicSocketEntry::OnCanWrite()
{

}
void QuicSocketEntry::OnCanRead()
{

}
void QuicSocketEntry::OnError()
{

}
void QuicSocketEntry::OnAccept(int fd, const char * host, int port)
{

}

} // namespace posix_quic
