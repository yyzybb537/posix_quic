#include "socket_entry.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include "clock.h"
#include "task_runner.h"

namespace posix_quic {

using namespace net;

QuartcFactory& QuicSocketEntry::GetQuartcFactory()
{
    static QuartcFactory factory(
            QuartcFactoryConfig{&QuicTaskRunner::getInstance(), &QuicClockImpl::getInstance()},
            []( std::unique_ptr<QuicConnection> connection,
                const QuicConfig& config,
                const std::string& unique_remote_server_id,
                Perspective perspective,
                QuicConnectionHelperInterface* helper,
                QuicClock* clock)
            {
                return static_cast<QuartcSessionInterface*>(new QuicSocketEntry(
                        std::move(connection), config, unique_remote_server_id,
                        perspective, helper, clock));
            });
    return factory;
}

QuicSocketEntryPtr QuicSocketEntry::NewQuicSocketEntry()
{
    return NewQuicSocketEntry(QuicRandom::GetInstance()->RandUint64());
}

QuicSocketEntryPtr QuicSocketEntry::NewQuicSocketEntry(QuicConnectionId id)
{
    int fd = GetFdFactory().Alloc();
    std::shared_ptr<PacketTransport> packetTransport(new PacketTransport);
    QuartcSessionConfig config;
    config.unique_remote_server_id = "";
    config.packet_transport = packetTransport.get();
    config.congestion_control = QuartcCongestionControl::kBBR;
    config.connection_id = id;
    config.max_idle_time_before_crypto_handshake_secs = 10;
    config.max_time_before_crypto_handshake_secs = 10;
    QuartcSessionInterface* ptr = GetQuartcFactory().CreateQuartcSession(config).release();

    QuicSocketEntryPtr sptr((QuicSocketEntry*)ptr);
    sptr->SetFd(fd);
    sptr->packetTransport_ = packetTransport;
    sptr->SetDelegate(sptr.get());

    GetFdManager().Put(fd, sptr);
    return sptr;
}

void QuicSocketEntry::DeleteQuicSocketEntry(QuicSocketEntryPtr ptr)
{
    int fd = ptr->Fd();
    if (fd >= 0) {
        GetFdManager().Delete(fd);
        GetFdFactory().Free(fd);
        ptr->SetFd(-1);
    }
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
    GetConnectionManager().Put(*udpSocket_, connection_id(), Fd(), true);
    return 0;
}

ConnectionManager & QuicSocketEntry::GetConnectionManager()
{
    static ConnectionManager obj;
    return obj;
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
            errno = EALREADY;
            return -1;

        default:
            errno = EINVAL;
            return -1;
    }

    socketState_ = QuicSocketState_Connecting;

    QuicSocketAddress address(*reinterpret_cast<sockaddr_storage*>(addr));

    packetTransport_->Set(udpSocket_, address);

    this->StartCryptoHandshake();

    errno = EINPROGRESS;
    return -1;
}
int QuicSocketEntry::Close()
{
    socketState_ = QuicSocketState_Closed;
    if (udpSocket_)
        GetConnectionManager().Delete(*udpSocket_, connection_id(), Fd());
    CloseConnection("");
    return 0;
}
QuicSocketEntryPtr QuicSocketEntry::AcceptSocket()
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
QuicStreamEntryPtr QuicSocketEntry::AcceptStream()
{
    if (!IsConnected()) {
        errno = ENOTCONN;
        return QuicStreamEntryPtr();
    }

    std::unique_lock<std::mutex> lock(streamQueueMtx_);
    while (!streamQueue_.empty()) {
        QuicStreamId id = streamQueue_.front();
        streamQueue_.pop();

        if (IsClosedStream(id))
            continue;

        return QuicStreamEntry::NewQuicStream(shared_from_this(), id);
    }

    errno = EAGAIN;
    return QuicStreamEntryPtr();
}

QuicStreamEntryPtr QuicSocketEntry::CreateStream()
{
    if (!IsConnected()) {
        errno = ENOTCONN;
        return QuicStreamEntryPtr();
    }

    std::unique_lock<std::mutex> lock(mtx_);
    QuicStreamId id = GetNextOutgoingStreamId();
    GetOrCreateStream(id);
    return QuicStreamEntry::NewQuicStream(shared_from_this(), id);
}

QuartcStreamPtr QuicSocketEntry::GetQuartcStream(QuicStreamId streamId)
{
    std::unique_lock<std::mutex> lock(mtx_);
    QuicStream* ptr = GetOrCreateStream(streamId);
    if (!ptr) return QuartcStreamPtr();
    auto self = this->shared_from_this();
    lock.release();

    return QuartcStreamPtr(ptr, [self](QuicStream*){
                self->mtx_.unlock();
            });
}

void QuicSocketEntry::CloseStream(uint64_t streamId)
{
    std::unique_lock<std::mutex> lock(mtx_);
    QuartcSession::CloseStream(streamId);
}

void QuicSocketEntry::OnCanWrite()
{
    std::unique_lock<std::mutex> lock(mtx_);
    QuartcSession::OnCanWrite();
}

void QuicSocketEntry::ProcessUdpPacket(const QuicSocketAddress& self_address,
        const QuicSocketAddress& peer_address,
        const QuicReceivedPacket& packet)
{
    std::unique_lock<std::mutex> lock(mtx_);
    QuartcSession::ProcessUdpPacket(self_address, peer_address, packet);
}

bool QuicSocketEntry::IsConnected()
{
    return socketState_ == QuicSocketState_Connected;
}
std::shared_ptr<int> QuicSocketEntry::NativeUdpFd() const 
{
    return udpSocket_;
}

void QuicSocketEntry::PushAcceptQueue(QuicSocketEntryPtr entry)
{
    std::unique_lock<std::mutex> lock(acceptQueueMtx_);
    acceptQueue_.push_back(entry);
}

void QuicSocketEntry::OnSyn(QuicSocketEntryPtr owner)
{
    socketState_ = QuicSocketState_Shared;
    udpSocket_ = owner.udpSocket_;
    GetConnectionManager().Put(*udpSocket_, connection_id(), Fd(), false);
}

void QuicSocketEntry::OnCryptoHandshakeComplete()
{
    socketState_ = QuicSocketState_Connected;
    SetWritable(true);
    if (perspective() == Perspective::IS_SERVER) {
        // push to owner accept queue.
    }
}

void QuicSocketEntry::OnIncomingStream(QuartcStreamInterface* stream)
{
    streamQueue_.push_back(stream->stream_id());
}

void QuicSocketEntry::OnConnectionClosed(int error_code, bool from_remote)
{
    socketState_ = QuicSocketState_Closed;
    SetCloseByPeer(from_remote);
    SetError(ESHUTDOWN, error_code);
}

} // namespace posix_quic
