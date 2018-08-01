#include "quic_socket.h"
#include "socket_entry.h"
#include "stream_entry.h"
#include "epoller_entry.h"

namespace posix_quic {

QuicSocket QuicCreateSocket()
{
    auto socket = QuicSocketEntry::NewQuicSocketEntry();
    return socket->Fd();
}

int QuicCloseSocket(QuicSocket sock)
{
    EntryPtr entry = EntryBase::GetFdManager().Get(sock);
    if (!entry || entry->Category() != EntryCategory::Socket) {
        errno = EBADF;
        return -1;
    }

    auto socket = std::dynamic_pointer_cast<QuicSocketEntry>(entry);
    socket->Close();
    QuicSocketEntry::DeleteQuicSocketEntry(socket);
    return 0;
}

int QuicBind(QuicSocket sock, const struct sockaddr* addr, socklen_t addrlen)
{
    auto socket = EntryBase::GetFdManager().Get(sock);
    if (!socket || socket->Category() != EntryCategory::Socket) {
        errno = EBADF;
        return -1;
    }

    return ((QuicSocketEntry*)socket.get())->Bind(addr, addrlen);
}

int QuicListen(QuicSocket sock, int backlog)
{
    // TODO: limit acceptQueue size.
    return 0;
}

int QuicConnect(QuicSocket sock, const struct sockaddr* addr, socklen_t addrlen)
{
    auto socket = EntryBase::GetFdManager().Get(sock);
    if (!socket || socket->Category() != EntryCategory::Socket) {
        errno = EBADF;
        return -1;
    }

    return ((QuicSocketEntry*)socket.get())->Connect(addr, addrlen);
}

bool IsConnected(QuicSocket sock)
{
    auto socket = EntryBase::GetFdManager().Get(sock);
    if (!socket || socket->Category() != EntryCategory::Socket) {
        errno = EBADF;
        return -1;
    }

    return ((QuicSocketEntry*)socket.get())->IsConnected();
}

EntryCategory GetCategory(int fd)
{
    auto socket = EntryBase::GetFdManager().Get(fd);
    if (!socket) return EntryCategory::Invalid;
    return socket->Category();
}

QuicSocket QuicSocketAccept(QuicSocket listenSock, const struct sockaddr* addr, socklen_t & addrlen)
{
    auto socket = EntryBase::GetFdManager().Get(listenSock);
    if (!socket || socket->Category() != EntryCategory::Socket) {
        errno = EBADF;
        return -1;
    }

    auto newSocket = ((QuicSocketEntry*)socket.get())->AcceptSocket();
    if (!newSocket) return -1;

    return newSocket->Fd();
}

QuicStream QuicStreamAccept(QuicSocket sock)
{
    auto socket = EntryBase::GetFdManager().Get(sock);
    if (!socket || socket->Category() != EntryCategory::Socket) {
        errno = EBADF;
        return -1;
    }

    auto stream = ((QuicSocketEntry*)socket.get())->AcceptStream();
    if (!stream) return -1;

    return stream->Fd();
}

QuicStream QuicCreateStream(QuicSocket sock)
{
    auto socket = EntryBase::GetFdManager().Get(sock);
    if (!socket || socket->Category() != EntryCategory::Socket) {
        errno = EBADF;
        return -1;
    }

    auto stream = ((QuicSocketEntry*)socket.get())->CreateStream();
    if (!stream) return -1;

    return stream->Fd();
}

int QuicCloseStream(QuicStream stream)
{
    auto entry = EntryBase::GetFdManager().Get(stream);
    if (!entry || entry->Category() != EntryCategory::Stream) {
        errno = EBADF;
        return -1;
    }

    entry->Close();
    auto streamPtr = std::dynamic_pointer_cast<QuicStreamEntry>(entry);
    QuicStreamEntry::DeleteQuicStream(streamPtr);
    return 0;
}

ssize_t QuicWritev(QuicStream stream, const struct iovec* iov, int iov_count, bool fin)
{
    auto streamPtr = EntryBase::GetFdManager().Get(stream);
    if (!streamPtr || streamPtr->Category() != EntryCategory::Stream) {
        errno = EBADF;
        return -1;
    }

    return ((QuicStreamEntry*)streamPtr.get())->Writev(iov, iov_count, fin);
}

ssize_t QuicWrite(QuicStream stream, const void* data, size_t length, bool fin)
{
    struct iovec iov;
    iov.iov_base = (void*)data;
    iov.iov_len = length;
    return QuicWritev(stream, &iov, 1, fin);
}

ssize_t QuicReadv(QuicStream stream, const struct iovec* iov, int iov_count)
{
    auto streamPtr = EntryBase::GetFdManager().Get(stream);
    if (!streamPtr || streamPtr->Category() != EntryCategory::Stream) {
        errno = EBADF;
        return -1;
    }

    return ((QuicStreamEntry*)streamPtr.get())->Readv(iov, iov_count);
}

ssize_t QuicRead(QuicStream stream, void* data, size_t length)
{
    struct iovec iov;
    iov.iov_base = data;
    iov.iov_len = length;
    return QuicReadv(stream, &iov, 1);
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
    QuicEpollerEntryPtr ep = QuicEpollerEntry::NewQuicEpollerEntry();
    return ep->Fd();
}

int QuicCloseEpoller(QuicEpoller epfd)
{
    QuicEpollerEntryPtr ep = QuicEpollerEntry::GetFdManager().Get(epfd);
    if (!ep) {
        errno = EBADF;
        return -1;
    }

    QuicEpollerEntry::DeleteQuicEpollerEntry(ep);
    return 0;
}

int QuicEpollCtl(QuicEpoller epfd, int op, int quicFd, struct epoll_event *event)
{
    QuicEpollerEntryPtr ep = QuicEpollerEntry::GetFdManager().Get(epfd);
    if (!ep) {
        errno = EBADF;
        return -1;
    }

    switch (op) {
        case EPOLL_CTL_ADD:
            return ep->Add(quicFd, event);

        case EPOLL_CTL_MOD:
            return ep->Mod(quicFd, event);

        case EPOLL_CTL_DEL:
            return ep->Del(quicFd);
            
        default:
            errno = EINVAL;
            return -1;
    }
}

int QuicEpollWait(QuicEpoller epfd, struct epoll_event *events, int maxevents, int timeout)
{
    QuicEpollerEntryPtr ep = QuicEpollerEntry::GetFdManager().Get(epfd);
    if (!ep) {
        errno = EBADF;
        return -1;
    }

    return ep->Wait(events, maxevents, timeout);
}

} // namespace posix_quic
