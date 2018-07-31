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
    auto socket = EntryBase::GetFdManager().Get(sock);
    if (!socket) return ;
    if (socket->EntryCategory() != EntryCategory::Socket) return ;

    socket->Close();
    QuicSocketEntry::DeleteQuicSocketEntry(socket);
}

int QuicBind(QuicSocket sock, const struct sockaddr* addr, socklen_t addrlen)
{
    auto socket = EntryBase::GetFdManager().Get(sock);
    if (!socket || socket->EntryCategory() != EntryCategory::Socket) {
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
    auto socket = EntryBase::GetFdManager().Get(sock);
    if (!socket || socket->EntryCategory() != EntryCategory::Socket) {
        errno = EBADF;
        return -1;
    }

    return socket->Connect(addr, addrlen);
}

QuicSocket QuicSocketAccept(QuicSocket listenSock, const struct sockaddr* addr, socklen_t & addrlen)
{
    auto socket = EntryBase::GetFdManager().Get(listenSock);
    if (!socket || socket->EntryCategory() != EntryCategory::Socket) {
        errno = EBADF;
        return -1;
    }

    auto newSocket = socket->AcceptSocket();
    if (!newSocket) return -1;

    return newSocket->Fd();
}

QuicStream QuicStreamAccept(QuicSocket sock)
{
    auto socket = EntryBase::GetFdManager().Get(sock);
    if (!socket || socket->EntryCategory() != EntryCategory::Socket) {
        errno = EBADF;
        return -1;
    }

    auto stream = socket->AcceptStream();
    if (!stream) return -1;

    return stream->Fd();
}

QuicStream QuicCreateStream(QuicSocket sock)
{
    auto socket = EntryBase::GetFdManager().Get(sock);
    if (!socket || socket->EntryCategory() != EntryCategory::Socket) {
        errno = EBADF;
        return -1;
    }

    auto stream = socket->CreateStream();
    if (!stream) return -1;

    return stream->Fd();
}

void QuicCloseStream(QuicStream stream)
{
    auto streamPtr = EntryBase::GetFdManager().Get(stream);
    if (!streamPtr) return ;
    if (streamPtr->EntryCategory() != EntryCategory::Stream) {

    streamPtr->Close();
    QuicStreamEntry::DeleteQuicStream(streamPtr);
}

ssize_t QuicWritev(QuicStream stream, const struct iovec* iov, int iov_count,
        bool fin)
{
    auto streamPtr = EntryBase::GetFdManager().Get(stream);
    if (!streamPtr || streamPtr->EntryCategory() != EntryCategory::Stream) {
        errno = EBADF;
        return -1;
    }

    return streamPtr->Writev(iov, iov_count, fin);
}

ssize_t QuicReadv(QuicStream stream, const struct iovec* iov, int iov_count)
{
    auto streamPtr = EntryBase::GetFdManager().Get(stream);
    if (!streamPtr || streamPtr->EntryCategory() != EntryCategory::Stream) {
        errno = EBADF;
        return -1;
    }

    return streamPtr->Readv(iov, iov_count);
}

// poll
int QuicPoll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    if (!nfds) {
        usleep(timeout * 1000);
        return -1;
    }

    if (!timeout) {
        int res = 0;
        for (nfds_t i = 0; i < nfds; i++) {
            struct pollfd & pfd = fds[i];
            pfd.revents = 0;
            if (pfd.events == 0)
                continue;

            if (pfd.fd < 0)
                continue;

            auto entry = EntryBase::GetFdManager().Get(pfd.fd);
            if ((pfd.events & POLLIN) && entry->Readable())
                pfd.revents |= POLLIN;
            if ((pfd.events & POLLOUT) && entry->Writable())
                pfd.revents |= POLLOUT;
            if (entry->Error())
                pfd.revents |= POLLERR;

            if (pfd.revents != 0)
                ++res;
        }
        return res;
    }

    std::vector<EntryPtr> entries(nfds);
    Event::EventTrigger trigger;
    int res = 0;
    int events = 0;
    for (nfds_t i = 0; i < nfds; i++) {
        struct pollfd & pfd = fds[i];
        pfd.revents = 0;
        if (pfd.events == 0) {
            continue;
        }

        if (pfd.fd < 0) {
            continue;
        }

        Event::EventWaiter waiter = { &pfd.events, &pfd.revents };

        auto entry = EntryBase::GetFdManager().Get(pfd.fd);
        events |= entry->StartWait(waiter, &trigger);
        entries.push_back(entry);
    }

    if (events == 0) {
        // not triggered, wait it.
        trigger.Wait(timeout);
    }

    for (auto & entry : entries) {
        entry->StopWait(&trigger);
    }

    entries.clear();

    int res = 0;
    for (nfds_t i = 0; i < nfds; i++) {
        struct pollfd & pfd = fds[i];
        if (pfd.revents != 0)
            ++res;
    }
    return res;
}

// epoll
QuicEpoller QuicCreateEpoll()
{

}

void QuicCloseEpoller(QuicEpoller epfd)
{

}

int QuicEpollCtl(QuicEpoller epfd, int op, int quicFd, struct epoll_event *event)
{

}

int QuicEpollWait(QuicEpoller epfd, struct epoll_event *events, int maxevents, int timeout)
{

}

} // namespace posix_quic
