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
#include "stream_entry.h"

namespace posix_quic {

using namespace net;

QuicSocketEntry::QuicSocketEntry(QuicSocketSession * session)
    : impl_(session)
{
}

QuicSocketEntry::~QuicSocketEntry()
{
    impl_.reset();
}

QuicSocketEntryPtr QuicSocketEntry::NewQuicSocketEntry(bool isServer, QuicConnectionId id)
{
    if (id == INVALID_QUIC_CONNECTION_ID)
        id = QuicRandom::GetInstance()->RandUint64();

    std::shared_ptr<QuicTaskRunnerProxy> taskRunnerProxy(new QuicTaskRunnerProxy);
    taskRunnerProxy->SetConnectionId(id);

    std::shared_ptr<QuartcFactory> factory(new QuartcFactory(
            QuartcFactoryConfig{taskRunnerProxy.get(), &QuicClockImpl::getInstance()},
            []( std::unique_ptr<QuicConnection> connection,
                const QuicConfig& config, const std::string& unique_remote_server_id,
                Perspective perspective,
                QuicConnectionHelperInterface* helper,
                QuicClock* clock )
            {
                return static_cast<QuartcSessionInterface*>(new QuicSocketSession(
                        std::move(connection), config, unique_remote_server_id,
                        perspective, helper, clock));
            }));

    int fd = GetFdFactory().Alloc();
    std::shared_ptr<PosixQuicPacketTransport> packetTransport(new PosixQuicPacketTransport);
    QuartcFactoryInterface::QuartcSessionConfig config;
    config.is_server = isServer;
    config.unique_remote_server_id = "";
    config.packet_transport = packetTransport.get();
    config.congestion_control = QuartcCongestionControl::kBBR;
    config.connection_id = id;
    config.max_idle_time_before_crypto_handshake_secs = 10;
    config.max_time_before_crypto_handshake_secs = 10;
    QuicSocketSession* session = (QuicSocketSession*)factory->CreateQuartcSession(config).release();

    QuicSocketEntryPtr sptr(new QuicSocketEntry(session));
    sptr->SetFd(fd);
    sptr->factory_ = factory;
    sptr->packetTransport_ = packetTransport;
    sptr->taskRunnerProxy_ = taskRunnerProxy;
    session->SetDelegate(sptr.get());
    session->connection()->set_debug_visitor(&sptr->connectionVisitor_);
    sptr->connectionVisitor_.Bind(session->connection(), &sptr->opts_,
            sptr.get(), sptr->packetTransport_);

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

void QuicSocketEntry::ClearAcceptSocketByClose()
{
    if (!acceptSockets_.empty()) {
        std::unique_lock<std::mutex> lock(acceptSocketsMtx_);
        while (!acceptSockets_.empty()) {
            auto socket = acceptSockets_.front();
            acceptSockets_.pop_front();
            socket->Close();
            DeleteQuicSocketEntry(socket);
        }
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
    GetConnectionManager().Put(*udpSocket_, impl_->connection_id(), Fd(), true);
    return 0;
}

ConnectionManager & QuicSocketEntry::GetConnectionManager()
{
    static ConnectionManager obj;
    return obj;
}

// 链接建立前设置才有效, 握手协商后就直接取协商值使用了.
void QuicSocketEntry::SetOpt(int type, long value)
{
    switch (type) {
        case sockopt_idle_timeout_secs:
            if (value < kMinIdleTimeout) {
                value = kMinIdleTimeout;
            }
            break;
    }

    if (!opts_.SetOption(type, value)) return ;

    switch (type) {
        case sockopt_ack_timeout_secs:
            {
                std::unique_lock<std::recursive_mutex> lock(mtx_);
                connectionVisitor_.SetNoAckAlarm();
            }
            break;

        case sockopt_idle_timeout_secs:
            {
                std::unique_lock<std::recursive_mutex> lock(mtx_);
                impl_->config()->SetIdleNetworkTimeout(QuicTime::Delta::FromSeconds(value),
                        QuicTime::Delta::FromSeconds(value));
                impl_->connection()->SetFromConfig(*impl_->config());
            }
            break;
    }
}

long QuicSocketEntry::GetOpt(int type) const
{
    return opts_.GetOption(type);
}

int QuicSocketEntry::Bind(const struct sockaddr* addr, socklen_t addrlen)
{
    switch (socketState_) {
        case QuicSocketState_None:
            {
                int res = CreateNewUdpSocket();
                if (res < 0) return -1;
            }

        case QuicSocketState_Inited:
            {
               int res = ::bind(*udpSocket_, addr, addrlen);
               if (res == 0)
                   socketState_ = QuicSocketState_Binded;
               return res;
            }

        default:
            errno = EINVAL;
            return -1;
    }
}
int QuicSocketEntry::Connect(const struct sockaddr* addr, socklen_t addrlen)
{
    DebugPrint(dbg_connect, "begin. fd = %d", Fd());

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

    struct sockaddr_storage addr_s = {};
    memcpy(&addr_s, addr, addrlen);
    QuicSocketAddress address(addr_s);

    packetTransport_->Set(udpSocket_, address);

    DebugPrint(dbg_connect, "-> fd = %d, StartCryptoHandshake connectionId = %lu", Fd(), impl_->connection_id());
    impl_->OnTransportCanWrite();
    impl_->Initialize();
    if (GetOpt(sockopt_ack_timeout_secs) == 0 && kDefaultAckTimeout)
        this->SetOpt(sockopt_ack_timeout_secs, kDefaultAckTimeout);
    impl_->StartCryptoHandshake();

    errno = EINPROGRESS;
    return -1;
}
int QuicSocketEntry::Close()
{
    if (socketState_ == QuicSocketState_Closed)
        return 0;

    std::unique_lock<std::recursive_mutex> lock(mtx_);
    if (socketState_ == QuicSocketState_Closed)
        return 0;

    socketState_ = QuicSocketState_Closed;
    connectionVisitor_.CancelNoAckAlarm();
    if (udpSocket_)
        GetConnectionManager().Delete(*udpSocket_, impl_->connection_id(), Fd());
    impl_->CloseConnection("");
    ClearWaitingsByClose();
    ClearAcceptSocketByClose();
    return 0;
}
QuicSocketEntryPtr QuicSocketEntry::AcceptSocket()
{
    if (socketState_ != QuicSocketState_Binded) {
        errno = EINVAL;
        return QuicSocketEntryPtr();
    }

    std::unique_lock<std::mutex> lock(acceptSocketsMtx_);
    if (acceptSockets_.empty()) {
        errno = EAGAIN;
        return QuicSocketEntryPtr();
    }

    auto ptr = acceptSockets_.front();
    acceptSockets_.pop_front();
    return ptr;
}
QuicStreamEntryPtr QuicSocketEntry::AcceptStream()
{
    if (!IsConnected()) {
        errno = ENOTCONN;
        return QuicStreamEntryPtr();
    }

    std::unique_lock<std::mutex> lock(acceptStreamsMtx_);
    while (!acceptStreams_.empty()) {
        QuicStreamEntryPtr stream = acceptStreams_.front();
        acceptStreams_.pop_front();
        return stream;
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

    std::unique_lock<std::recursive_mutex> lock(mtx_);
    QuartcStream* stream = impl_->CreateOutgoingDynamicStream();
    return QuicStreamEntry::NewQuicStream(shared_from_this(), stream);
}

QuartcStreamPtr QuicSocketEntry::GetQuartcStream(QuicStreamId streamId)
{
    std::unique_lock<std::recursive_mutex> lock(mtx_);
    QuartcStream* ptr = (QuartcStream*)impl_->GetOrCreateStream(streamId);
    if (!ptr) return QuartcStreamPtr();
    auto self = this->shared_from_this();
    lock.release();

    return QuartcStreamPtr(ptr, [self](QuartcStream*){
                self->mtx_.unlock();
            });
}

void QuicSocketEntry::CloseStream(uint64_t streamId)
{
    std::unique_lock<std::recursive_mutex> lock(mtx_);
    impl_->CloseStream(streamId);
}

void QuicSocketEntry::ProcessUdpPacket(const QuicSocketAddress& self_address,
        const QuicSocketAddress& peer_address,
        const QuicReceivedPacket& packet)
{
    TlsConnectionIdGuard guard(impl_->connection_id());

    DebugPrint(dbg_connect | dbg_accept | dbg_write | dbg_read,
            "fd = %d, packet length = %d", Fd(), (int)packet.length());
    std::unique_lock<std::recursive_mutex> lock(mtx_);
    impl_->FlushWrites();
    impl_->ProcessUdpPacket(self_address, peer_address, packet);
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
    DebugPrint(dbg_accept, "this->fd = %d, newSocket->fd = %d", Fd(), entry->Fd());
    std::unique_lock<std::mutex> lock(acceptSocketsMtx_);
    acceptSockets_.push_back(entry);
    SetReadable(true);
}

void QuicSocketEntry::OnSyn(QuicSocketEntryPtr owner, QuicSocketAddress address)
{
    socketState_ = QuicSocketState_Shared;
    udpSocket_ = owner->udpSocket_;
    packetTransport_->Set(udpSocket_, address);
    GetConnectionManager().Put(*udpSocket_, impl_->connection_id(), Fd(), false);
    DebugPrint(dbg_accept, "-> fd = %d, connectionId = %lu, OnSyn(owner=%d, address=%s)",
            Fd(), impl_->connection_id(), owner->Fd(), address.ToString().c_str());
    impl_->OnTransportCanWrite();
    impl_->Initialize();
    if (GetOpt(sockopt_ack_timeout_secs) == 0 && kDefaultAckTimeout)
        this->SetOpt(sockopt_ack_timeout_secs, kDefaultAckTimeout);
    if (GetOpt(sockopt_idle_timeout_secs) == 0 && kDefaultIdleTimeout)
        this->SetOpt(sockopt_idle_timeout_secs, kDefaultIdleTimeout);
    impl_->StartCryptoHandshake();
}

void QuicSocketEntry::OnCryptoHandshakeComplete()
{
    socketState_ = QuicSocketState_Connected;
    DebugPrint(dbg_connect | dbg_accept, "-> fd = %d, OnCryptoHandshakeComplete %s", 
            Fd(), Perspective2Str((int)impl_->perspective()));
    SetWritable(true);
    if (impl_->perspective() == Perspective::IS_SERVER) {
        // push to owner accept queue.
        assert(udpSocket_);
        QuicSocket ownerSocket = GetConnectionManager().GetOwner(*udpSocket_);
        EntryPtr owner = GetFdManager().Get(ownerSocket);
        assert(owner->Category() == EntryCategory::Socket);

        ((QuicSocketEntry*)owner.get())->PushAcceptQueue(shared_from_this());
    }
}

void QuicSocketEntry::OnIncomingStream(QuartcStreamInterface* stream)
{
    QuicStreamEntryPtr streamPtr = QuicStreamEntry::NewQuicStream(shared_from_this(), stream);
    std::unique_lock<std::mutex> lock(acceptStreamsMtx_);
    acceptStreams_.push_back(streamPtr);
    SetReadable(true);
}

void QuicSocketEntry::OnConnectionClosed(int error_code, bool from_remote)
{
    SetCloseByPeer(from_remote);
    SetError(ESHUTDOWN, error_code);

    if (socketState_ != QuicSocketState_Connected && impl_->perspective() == Perspective::IS_SERVER) {
        // 接受链接失败, fd还没吐给用户层
        this->Close();
        DeleteQuicSocketEntry(shared_from_this());
    }
}

std::string QuicSocketEntry::GetDebugInfo(int indent)
{
    std::string idt(indent, ' ');
    std::string idt2 = idt + ' ';
    std::string idt3 = idt2 + ' ';

    std::unique_lock<std::recursive_mutex> lock(mtx_);
    std::string info;
    info += idt + P("=========== Socket: %d ===========", Fd());

    if (!acceptSockets_.empty()) {
        std::unique_lock<std::mutex> lock(acceptSocketsMtx_);
        if (!acceptSockets_.empty()) {
            info += idt2 + P("accept socket list: (count = %d)", (int)acceptSockets_.size());

            for (auto & socket : acceptSockets_) {
                info += idt3 + P("-> fd = %d", socket->Fd());
            }
        }
    }

    if (!acceptStreams_.empty()) {
        std::unique_lock<std::mutex> lock(acceptStreamsMtx_);
        if (!acceptStreams_.empty()) {
            info += idt2 + P("accept stream list: (count = %d)", (int)acceptStreams_.size());

            for (auto & stream : acceptStreams_) {
                info += idt3 + P("-> fd = %d", stream->Fd());
            }
        }
    }

    size_t numInStreams = impl_->GetNumOpenIncomingStreams();
    size_t numOutStreams = impl_->GetNumOpenOutgoingStreams();

    if (numInStreams > 0)
        info += idt2 + P("Number incoming stream: %d", (int)numInStreams);

    if (numOutStreams > 0)
        info += idt2 + P("Number outgoing stream: %d", (int)numOutStreams);

    info += Event::GetDebugInfo(indent + 1);
    return info;
}

QuicConnectionId QuicSocketEntry::connection_id()
{
    return impl_ ? impl_->connection_id() : -1;
}

} // namespace posix_quic
