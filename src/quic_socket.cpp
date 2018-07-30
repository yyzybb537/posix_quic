#include "quic_socket.h"
#include "socket_entry.h"
#include "stream_entry.h"

namespace posix_quic {

QuicSocket QuicCreateSocket()
{
    auto socket = QuicSocketEntry::NewQuicSocketEntry();
    return socket->Fd();
}

void QuicCloseSocket(QuicSocket sock)
{
    auto socket = QuicSocketEntry::GetFdManager().Get(sock);
    if (!socket) return ;

    socket->Close();
    QuicSocketEntry::DeleteQuicSocketEntry(socket);
}

int QuicBind(QuicSocket sock, const struct sockaddr* addr, socklen_t addrlen)
{
    auto socket = QuicSocketEntry::GetFdManager().Get(sock);
    if (!socket) {
        errno = EBADF;
        return -1;
    }

    return socket->Bind(addr, addrlen);
}

int QuicListen(QuicSocket sock, int backlog)
{
    // TODO: limit acceptQueue size.
    return 0;
}

int QuicConnect(QuicSocket sock, const struct sockaddr* addr, socklen_t addrlen)
{
    auto socket = QuicSocketEntry::GetFdManager().Get(sock);
    if (!socket) {
        errno = EBADF;
        return -1;
    }

    return socket->Connect(addr, addrlen);
}

QuicSocket QuicSocketAccept(QuicSocket listenSock, const struct sockaddr* addr, socklen_t & addrlen)
{
    auto socket = QuicSocketEntry::GetFdManager().Get(listenSock);
    if (!socket) {
        errno = EBADF;
        return -1;
    }

    auto newSocket = socket->AcceptSocket();
    if (!newSocket) return -1;

    return newSocket->Fd();
}

QuicStream QuicStreamAccept(QuicSocket sock)
{
    auto socket = QuicSocketEntry::GetFdManager().Get(sock);
    if (!socket) {
        errno = EBADF;
        return -1;
    }

    auto stream = socket->AcceptStream();
    if (!stream) return -1;

    return stream->Fd();
}

QuicStream QuicCreateStream(QuicSocket sock)
{
    auto socket = QuicSocketEntry::GetFdManager().Get(sock);
    if (!socket) {
        errno = EBADF;
        return -1;
    }

    auto stream = socket->CreateStream();
    if (!stream) return -1;

    return stream->Fd();
}

void QuicCloseStream(QuicStream stream)
{
    auto streamPtr = QuicStreamEntry::GetFdManager().Get(stream);
    if (!streamPtr) return ;

    streamPtr->Close();
    QuicStreamEntry::DeleteQuicStream(streamPtr);
}

ssize_t QuicWritev(QuicStream stream, const struct iovec* iov, int iov_count,
        bool fin)
{
    auto streamPtr = QuicStreamEntry::GetFdManager().Get(stream);
    if (!streamPtr) {
        errno = EBADF;
        return -1;
    }

    return streamPtr->Writev(iov, iov_count, fin);
}

ssize_t QuicReadv(QuicStream stream, const struct iovec* iov, int iov_count)
{
    auto streamPtr = QuicStreamEntry::GetFdManager().Get(stream);
    if (!streamPtr) {
        errno = EBADF;
        return -1;
    }

    return streamPtr->Readv(iov, iov_count);
}

// poll
int QuicPoll(struct pollfd *fds, nfds_t nfds, int timeout)
{

}

// epoll
QuicEpoller QuicCreateEpoll();

void QuicCloseEpoller(QuicEpoller epfd);

int QuicEpollCtl(QuicEpoller epfd, int op, QuicStream stream, struct epoll_event *event);

int QuicEpollWait(QuicEpoller epfd, struct epoll_event *events, int maxevents, int timeout);

} // namespace posix_quic
