#include "connection.h"
#include "io_service.h"
#include <assert.h>

namespace posix_quic {
namespace simple {

Connection::Stream::Stream(Connection * connection, QuicStream stream)
{
    ctx_.isSocket = false;
    ctx_.connection = connection;
    ctx_.stream = stream;
    ctx_.id = connection->ios_->AddContext(&ctx_);

    struct epoll_event ev;
    ev.data.u64 = ctx_.id;
    ev.events = EPOLLIN | EPOLLOUT;
    int res = QuicEpollCtl(connection->ios_->Native(), EPOLL_CTL_ADD, stream, &ev);
    DebugPrint(dbg_simple, "fd = %d, Add stream = %d into service. returns %d",
            connection->Native(), stream, res);
}

Connection::Connection(IOService* ios)
    : ios_(ios)
{
    socket_ = QuicCreateSocket();
    connectionId_ = GetQuicConnectionId(socket_);

    sockCtx_.isSocket = true;
    sockCtx_.socket = socket_;
    sockCtx_.connection = this;
}

// server
Connection::Connection(IOService* ios, QuicSocket socket)
    : ios_(ios)
{
    socket_ = socket;
    connectionId_ = GetQuicConnectionId(socket_);
    connected_ = true;

    sockCtx_.isSocket = true;
    sockCtx_.socket = socket_;
    sockCtx_.connection = this;

    AddIntoService();
}

Connection::~Connection()
{
    Close();
    
    if (sockCtx_.id) {
        ios_->DelContext(sockCtx_.id);
    }
}

int Connection::Connect(const struct sockaddr* addr, socklen_t addrLen)
{
    QuicConnect(socket_, addr, addrLen);
    return AddIntoService();
}

int Connection::Accept(const struct sockaddr* addr, socklen_t addrLen)
{
    int res = QuicBind(socket_, addr, addrLen);
    if (res < 0)
        return res;

    listener_ = true;
    return AddIntoService();
}

bool Connection::Write(const char* data, size_t len, QuicStream stream, bool fin)
{
    if (!connected_) return false;

    StreamPtr streamPtr;
    switch (stream) {
        case PublicStream:
            if (streams_.empty()) {
                streamPtr = CreateOutgoingStream();
            } else {
                streamPtr = streams_.begin()->second;
            }
            break;

        case LastStream:
            if (streams_.empty()) {
                streamPtr = CreateOutgoingStream();
            } else {
                streamPtr = streams_.rbegin()->second;
            }
            break;

        case AlwaysNewStream:
            streamPtr = CreateOutgoingStream();
            break;

        default:
            streamPtr = GetStream(stream);
            break;
    }
    if (!streamPtr) return false;

    return streamPtr->Write(data, len, fin);
}

void Connection::Close()
{
    streams_.clear();

    if (closed_) return ;
    closed_ = true;
    QuicCloseSocket(socket_);
    OnClose(0, 0, false);
}

long Connection::BufferedSliceCount(QuicStream stream)
{
    StreamPtr streamPtr = GetStream(stream);
    if (!streamPtr) return -1;
    return streamPtr->buffers_.size();
}

Connection::Stream::~Stream()
{
    Close();

    if (ctx_.id) {
        ctx_.connection->ios_->DelContext(ctx_.id);
    }
}

void Connection::Stream::Close()
{
    if (closed_) return ;
    closed_ = true;
    int sysError = 0, quicError = 0, bFromRemote = false;
    GetQuicError(ctx_.stream, &sysError, &quicError, nullptr);

    DebugPrint(dbg_simple, "fd = %d, close stream = %d",
            ctx_.connection->Native(), ctx_.stream);

    ctx_.connection->OnStreamClose(sysError, quicError, ctx_.stream);
    QuicCloseStream(ctx_.stream);
}

bool Connection::Stream::Write(const char* data, size_t len, bool fin)
{
    if (bufferedFin_) return false;

    if (fin) bufferedFin_ = true;

    if (!buffers_.empty()) {
        buffers_.push_back(std::string(data, len));
        return true;
    }

    ssize_t res = QuicWrite(ctx_.stream, data, len, fin);
    if (res < 0) res = 0;

    if ((size_t)res < len) {
        buffers_.push_back(std::string(data + res, len - res));
    }

    return true;
}

void Connection::OnAccept()
{
    if (listener_) {
        OnAcceptSocket();
    } else {
        OnAcceptStream();
    }
}

void Connection::OnAcceptSocket()
{
    for (;;) {
        QuicSocket newSocket = QuicSocketAccept(socket_);
        if (newSocket < 0)
            break;

        DebugPrint(dbg_simple, "fd = %d, newSocket = %d", Native(), newSocket);

        Connection * connection = NewConnection(ios_, newSocket);
        this->OnAcceptSocket(connection);
    }
}

void Connection::OnAcceptStream()
{
    for (;;) {
        QuicStream newStream = QuicStreamAccept(socket_);
        if (newStream < 0)
            break;

        DebugPrint(dbg_simple, "fd = %d, stream = %d", Native(), newStream);

        CreateIncomingStream(newStream);
        OnCanRead(newStream);
    }
}

void Connection::CreateIncomingStream(QuicStream stream)
{
    assert(streams_.count(stream) == 0);

    StreamPtr streamPtr(new Stream(this, stream));
    streams_[stream] = streamPtr;

    DebugPrint(dbg_simple, "fd = %d, newStream = %d", Native(), stream);
}

Connection::StreamPtr Connection::CreateOutgoingStream()
{
    QuicStream stream = QuicCreateStream(socket_);
    assert(streams_.count(stream) == 0);

    DebugPrint(dbg_simple, "fd = %d, newStream = %d", Native(), stream);

    StreamPtr streamPtr(new Stream(this, stream));
    streams_[stream] = streamPtr;
    return streamPtr;
}
Connection::StreamPtr Connection::GetStream(QuicStream stream)
{
    auto itr = streams_.find(stream);
    if (itr != streams_.end())
        return itr->second;
    return StreamPtr();
}

bool Connection::OnCanRead(QuicStream stream)
{
    DebugPrint(dbg_simple, "fd = %d, stream = %d", Native(), stream);

    if (stream == -1) {
        this->OnAccept();
        return true;
    }

    char buf[10240];
    for (;;) {
        int res = QuicRead(stream, buf, sizeof(buf));
        if (res < 0) {
            if (errno == EAGAIN)
                return true;

            DebugPrint(dbg_simple, "fd = %d, stream = %d, QuicRead errno = %d, close stream.",
                    Native(), stream, errno);
            StreamPtr streamPtr = GetStream(stream);
            if (streamPtr)
                streamPtr->Close();
            return false;
        } else if (res == 0) {
            DebugPrint(dbg_simple, "fd = %d, stream = %d, QuicRead returns 0, shutdown stream.",
                    Native(), stream);
            QuicStreamShutdown(stream, SHUT_WR);
            return true;
        }

        DebugPrint(dbg_simple, "fd = %d, recv-length = %d", Native(), res);
        this->OnRecv(buf, res, stream);
    }
    return true;
}

bool Connection::OnCanWrite(QuicStream stream)
{
    DebugPrint(dbg_simple, "fd = %d, stream = %d", Native(), stream);

    if (stream == -1) {
        if (IsConnected(socket_) && !connected_) {
            connected_ = true;
            this->OnConnected();
        }
        return true;
    }

    StreamPtr streamPtr = GetStream(stream);
    if (streamPtr)
        streamPtr->WriteBufferedData();
    return true;
}

bool Connection::OnError(QuicStream stream)
{
    DebugPrint(dbg_simple, "fd = %d, stream = %d", Native(), stream);

    int sysError = 0, quicError = 0, bFromRemote = false;

    if (stream == -1) {
        streams_.clear();

        if (closed_) return false;
        closed_ = true;
        GetQuicError(socket_, &sysError, &quicError, &bFromRemote);
        QuicCloseSocket(socket_);
        OnClose(sysError, quicError, bFromRemote);
        return false;
    }

    StreamPtr streamPtr = GetStream(stream);
    if (streamPtr) {
        streamPtr->Close();
        streams_.erase(stream);
        return false;
    }
    return true;
}

void Connection::Stream::WriteBufferedData()
{
    while (!buffers_.empty()) {
        std::string & buf = buffers_.front();

        ssize_t res = QuicWrite(ctx_.stream, buf.c_str(), buf.size(), false);
        if (res < 0) break;

        if ((size_t)res >= buf.size()) {
            buffers_.pop_front();
        } else {
            buf.erase(0, res);
            break;
        }
    }

    if (buffers_.empty() && bufferedFin_ && !writeFin_) {
        writeFin_ = true;
        QuicStreamShutdown(ctx_.stream, SHUT_WR);
    }
}

int Connection::AddIntoService()
{
    sockCtx_.id = ios_->AddContext(&sockCtx_);

    struct epoll_event ev;
    ev.data.u64 = sockCtx_.id;
    ev.events = EPOLLIN | EPOLLOUT;
    int res = QuicEpollCtl(ios_->Native(), EPOLL_CTL_ADD, socket_, &ev);
    DebugPrint(dbg_simple, "fd = %d, AddIntoService returns %d", Native(), res);
    return res;
}

} // amespace simple
} // namespace posix_quic
