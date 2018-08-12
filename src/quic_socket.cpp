#include "quic_socket.h"
#include "socket_entry.h"
#include "stream_entry.h"
#include "epoller_entry.h"

namespace posix_quic {

QuicSocket QuicCreateSocket()
{
    auto socket = QuicSocketEntry::NewQuicSocketEntry(false);
    DebugPrint(dbg_api, "socket fd = %d", socket->Fd());
    return socket->Fd();
}

int QuicCloseSocket(QuicSocket sock)
{
    EntryPtr entry = EntryBase::GetFdManager().Get(sock);
    if (!entry || entry->Category() != EntryCategory::Socket) {
        DebugPrint(dbg_api, "sock = %d, return = -1, errno = EBADF", sock);
        errno = EBADF;
        return -1;
    }

    auto socket = std::dynamic_pointer_cast<QuicSocketEntry>(entry);
    socket->Close();
    QuicSocketEntry::DeleteQuicSocketEntry(socket);
    DebugPrint(dbg_api, "sock = %d, return = 0", sock);
    return 0;
}

int GetQuicError(QuicSocketOrStream fd, int * sysError, int * quicError, int * bFromRemote)
{
    EntryPtr entry = EntryBase::GetFdManager().Get(fd);
    if (!entry) {
        DebugPrint(dbg_api, "fd = %d, return = -1, errno = EBADF", fd);
        errno = EBADF;
        return -1;
    }

    if (sysError)
        *sysError = entry->Error();
    if (quicError)
        *quicError = (int)entry->GetQuicErrorCode();
    if (bFromRemote)
        *bFromRemote = entry->IsCloseByPeer() ? 1 : 0;
    return 0;
}

const char* QuicErrorToString(int quicErrorCode)
{
    return net::QuicErrorCodeToString((QuicErrorCode)quicErrorCode);
}

int QuicBind(QuicSocket sock, const struct sockaddr* addr, socklen_t addrlen)
{
    auto socket = EntryBase::GetFdManager().Get(sock);
    if (!socket || socket->Category() != EntryCategory::Socket) {
        DebugPrint(dbg_api, "sock = %d, return = -1, errno = EBADF", sock);
        errno = EBADF;
        return -1;
    }

    int res = ((QuicSocketEntry*)socket.get())->Bind(addr, addrlen);
    DebugPrint(dbg_api, "sock = %d, return = %d, errno = %d", sock, res, errno);
    return res;
}

int QuicListen(QuicSocket sock, int backlog)
{
    // TODO: limit acceptQueue size.
    DebugPrint(dbg_api, "sock = %d, backlog = %d", sock, backlog);
    return 0;
}

int QuicConnect(QuicSocket sock, const struct sockaddr* addr, socklen_t addrlen)
{
    auto socket = EntryBase::GetFdManager().Get(sock);
    if (!socket || socket->Category() != EntryCategory::Socket) {
        DebugPrint(dbg_api, "sock = %d, return = -1, errno = EBADF", sock);
        errno = EBADF;
        return -1;
    }

    int res = ((QuicSocketEntry*)socket.get())->Connect(addr, addrlen);
    DebugPrint(dbg_api, "sock = %d, return = %d, errno = %d", sock, res, errno);
    return res;
}

bool IsConnected(QuicSocket sock)
{
    auto socket = EntryBase::GetFdManager().Get(sock);
    if (!socket || socket->Category() != EntryCategory::Socket) {
        DebugPrint(dbg_api, "sock = %d, return = -1, errno = EBADF", sock);
        errno = EBADF;
        return -1;
    }

    bool res = ((QuicSocketEntry*)socket.get())->IsConnected();
    DebugPrint(dbg_api, "sock = %d, return = %d, errno = %d", sock, res, errno);
    return res;
}

EntryCategory GetCategory(int fd)
{
    auto socket = EntryBase::GetFdManager().Get(fd);
    if (!socket) return EntryCategory::Invalid;
    return socket->Category();
}

QuicSocket QuicSocketAccept(QuicSocket listenSock)
{
    auto socket = EntryBase::GetFdManager().Get(listenSock);
    if (!socket || socket->Category() != EntryCategory::Socket) {
        DebugPrint(dbg_api, "sock = %d, return = -1, errno = EBADF", listenSock);
        errno = EBADF;
        return -1;
    }

    auto newSocket = ((QuicSocketEntry*)socket.get())->AcceptSocket();
    if (!newSocket) {
        DebugPrint(dbg_api, "sock = %d, return = -1, errno = %d", listenSock, errno);
        return -1;
    }

    DebugPrint(dbg_api, "sock = %d, newSocket = %d", listenSock, newSocket->Fd());
    return newSocket->Fd();
}

QuicStream QuicStreamAccept(QuicSocket sock)
{
    auto socket = EntryBase::GetFdManager().Get(sock);
    if (!socket || socket->Category() != EntryCategory::Socket) {
        DebugPrint(dbg_api, "sock = %d, return = -1, errno = EBADF", sock);
        errno = EBADF;
        return -1;
    }

    auto stream = ((QuicSocketEntry*)socket.get())->AcceptStream();
    if (!stream) {
        DebugPrint(dbg_api, "sock = %d, return = -1, errno = %d", sock, errno);
        return -1;
    }

    DebugPrint(dbg_api, "sock = %d, newStream = %d", sock, stream->Fd());
    return stream->Fd();
}

QuicStream QuicCreateStream(QuicSocket sock)
{
    auto socket = EntryBase::GetFdManager().Get(sock);
    if (!socket || socket->Category() != EntryCategory::Socket) {
        DebugPrint(dbg_api, "sock = %d, return = -1, errno = EBADF", sock);
        errno = EBADF;
        return -1;
    }

    auto stream = ((QuicSocketEntry*)socket.get())->CreateStream();
    if (!stream) {
        DebugPrint(dbg_api, "sock = %d, return = -1, errno = %d", sock, errno);
        return -1;
    }

    DebugPrint(dbg_api, "sock = %d, newStream = %d", sock, stream->Fd());
    return stream->Fd();
}

int QuicCloseStream(QuicStream stream)
{
    auto entry = EntryBase::GetFdManager().Get(stream);
    if (!entry || entry->Category() != EntryCategory::Stream) {
        DebugPrint(dbg_api, "stream = %d, return = -1, errno = EBADF", stream);
        errno = EBADF;
        return -1;
    }

    entry->Close();
    auto streamPtr = std::dynamic_pointer_cast<QuicStreamEntry>(entry);
    QuicStreamEntry::DeleteQuicStream(streamPtr);
    DebugPrint(dbg_api, "stream = %d, return = 0", stream);
    return 0;
}

int QuicStreamShutdown(QuicStream stream, int how)
{
    auto entry = EntryBase::GetFdManager().Get(stream);
    if (!entry || entry->Category() != EntryCategory::Stream) {
        DebugPrint(dbg_api, "stream = %d, return = -1, errno = EBADF", stream);
        errno = EBADF;
        return -1;
    }

    auto streamPtr = std::dynamic_pointer_cast<QuicStreamEntry>(entry);
    int res = streamPtr->Shutdown(how);
    DebugPrint(dbg_api, "stream = %d, return = 0", stream);
    return res;
}

ssize_t QuicWritev(QuicStream stream, const struct iovec* iov, int iov_count, bool fin)
{
    auto streamPtr = EntryBase::GetFdManager().Get(stream);
    if (!streamPtr || streamPtr->Category() != EntryCategory::Stream) {
        DebugPrint(dbg_api, "stream = %d, return = -1, errno = EBADF", stream);
        errno = EBADF;
        return -1;
    }

    ssize_t res = ((QuicStreamEntry*)streamPtr.get())->Writev(iov, iov_count, fin);
    DebugPrint(dbg_api, "stream = %d, return = %ld, errno = %d", stream, res, errno);
    return res;
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
        DebugPrint(dbg_api, "stream = %d, return = -1, errno = EBADF", stream);
        errno = EBADF;
        return -1;
    }

    ssize_t res = ((QuicStreamEntry*)streamPtr.get())->Readv(iov, iov_count);
    DebugPrint(dbg_api, "stream = %d, return = %ld, errno = %d", stream, res, errno);
    return res;
}

ssize_t QuicRead(QuicStream stream, void* data, size_t length)
{
    struct iovec iov;
    iov.iov_base = data;
    iov.iov_len = length;
    return QuicReadv(stream, &iov, 1);
}

int SetQuicSocketOpt(QuicSocket sock, int type, int64_t value)
{
    auto socket = EntryBase::GetFdManager().Get(sock);
    if (!socket || socket->Category() != EntryCategory::Socket) {
        DebugPrint(dbg_api, "sock = %d, return = -1, errno = EBADF", sock);
        errno = EBADF;
        return -1;
    }

    ((QuicSocketEntry*)socket.get())->SetOpt(type, value);
    errno = 0;
    return 0;
}

int GetQuicSocketOpt(QuicSocket sock, int type, int64_t* value)
{
    auto socket = EntryBase::GetFdManager().Get(sock);
    if (!socket || socket->Category() != EntryCategory::Socket) {
        DebugPrint(dbg_api, "sock = %d, return = -1, errno = EBADF", sock);
        errno = EBADF;
        return -1;
    }

    *value = ((QuicSocketEntry*)socket.get())->GetOpt(type);
    errno = 0;
    return 0;
}

// poll
//int QuicPoll(struct pollfd *fds, nfds_t nfds, int timeout)
//{
//    if (!nfds) {
//        usleep(timeout * 1000);
//        DebugPrint(dbg_api, "fds[0].fd = %d, nfds = %ld, timeout = %d, return = -1",
//                nfds > 0 ? fds[0].fd : -1, nfds, timeout);
//        return -1;
//    }
//
//    if (!timeout) {
//        int res = 0;
//        for (nfds_t i = 0; i < nfds; i++) {
//            struct pollfd & pfd = fds[i];
//            pfd.revents = 0;
//            if (pfd.events == 0)
//                continue;
//
//            if (pfd.fd < 0)
//                continue;
//
//            auto entry = EntryBase::GetFdManager().Get(pfd.fd);
//            if ((pfd.events & POLLIN) && entry->Readable())
//                pfd.revents |= POLLIN;
//            if ((pfd.events & POLLOUT) && entry->Writable())
//                pfd.revents |= POLLOUT;
//            if (entry->Error())
//                pfd.revents |= POLLERR;
//
//            if (pfd.revents != 0)
//                ++res;
//        }
//        DebugPrint(dbg_api, "fds[0].fd = %d, nfds = %ld, timeout = %d, return = %d, errno = %d",
//                nfds > 0 ? fds[0].fd : -1, nfds, timeout, res, errno);
//        return res;
//    }
//
//    std::vector<EntryPtr> entries(nfds);
//    Event::EventTrigger trigger;
//    int events = 0;
//    int nWaiting = 0;
//    for (nfds_t i = 0; i < nfds; i++) {
//        struct pollfd & pfd = fds[i];
//        pfd.revents = 0;
//        if (pfd.events == 0) {
//            continue;
//        }
//
//        if (pfd.fd < 0) {
//            continue;
//        }
//
//        Event::EventWaiter waiter = { &pfd.events, &pfd.revents };
//
//        auto entry = EntryBase::GetFdManager().Get(pfd.fd);
//        if (entry->StartWait(waiter, &trigger)) {
//            nWaiting++;
//            entries.push_back(entry);
//        }
//        events |= pfd.revents;
//    }
//
//    if (events == 0 && nWaiting) {
//        // not triggered, wait it.
//        trigger.Wait(timeout);
//    }
//
//    for (auto & entry : entries) {
//        entry->StopWait(&trigger);
//    }
//
//    entries.clear();
//
//    int res = 0;
//    for (nfds_t i = 0; i < nfds; i++) {
//        struct pollfd & pfd = fds[i];
//        if (pfd.revents != 0)
//            ++res;
//    }
//    DebugPrint(dbg_api, "fds[0].fd = %d, nfds = %ld, timeout = %d, return = %d, errno = %d",
//            nfds > 0 ? fds[0].fd : -1, nfds, timeout, res, errno);
//    return res;
//}

// epoll
QuicEpoller QuicCreateEpoll()
{
    QuicEpollerEntryPtr ep = QuicEpollerEntry::NewQuicEpollerEntry();
    DebugPrint(dbg_api, "epoll fd = %d", ep->Fd());
    return ep->Fd();
}

int QuicCloseEpoller(QuicEpoller epfd)
{
    QuicEpollerEntryPtr ep = QuicEpollerEntry::GetFdManager().Get(epfd);
    if (!ep) {
        DebugPrint(dbg_api, "epoll fd = %d, return = -1, errno = EBADF", epfd);
        errno = EBADF;
        return -1;
    }

    QuicEpollerEntry::DeleteQuicEpollerEntry(ep);
    DebugPrint(dbg_api, "epoll fd = %d, return = 0", epfd);
    return 0;
}

int QuicEpollCtl(QuicEpoller epfd, int op, int quicFd, struct epoll_event *event)
{
    QuicEpollerEntryPtr ep = QuicEpollerEntry::GetFdManager().Get(epfd);
    if (!ep) {
        DebugPrint(dbg_api, "epoll fd = %d, op = %s, quicFd = %d, return = -1, errno = EBADF",
                epfd, EpollOp2Str(op), quicFd);
        errno = EBADF;
        return -1;
    }

    int res;
    switch (op) {
        case EPOLL_CTL_ADD:
            res = ep->Add(quicFd, event);
            break;

        case EPOLL_CTL_MOD:
            res = ep->Mod(quicFd, event);
            break;

        case EPOLL_CTL_DEL:
            res = ep->Del(quicFd);
            break;
            
        default:
            errno = EINVAL;
            res = -1;
            break;
    }

    DebugPrint(dbg_api, "epoll fd = %d, op = %s, quicFd = %d, return = %d, errno = %d",
            epfd, EpollOp2Str(op), quicFd, res, errno);
    return res;
}

int QuicEpollWait(QuicEpoller epfd, struct epoll_event *events, int maxevents, int timeout)
{
    QuicEpollerEntryPtr ep = QuicEpollerEntry::GetFdManager().Get(epfd);
    if (!ep) {
        DebugPrint(dbg_api, "epoll fd = %d, return = -1, errno = EBADF", epfd);
        errno = EBADF;
        return -1;
    }

    errno = 0;
    int res = ep->Wait(events, maxevents, timeout);
    DebugPrint(dbg_api, "epoll fd = %d, return = %d, errno = %d", epfd, res, errno);
    return res;
}

int QuicNativeUdpSocket(QuicSocket sock)
{
    auto socket = EntryBase::GetFdManager().Get(sock);
    if (!socket || socket->Category() != EntryCategory::Socket) {
        DebugPrint(dbg_api, "sock = %d, return = -1", sock);
        return -1;
    }

    auto pUdp = ((QuicSocketEntry*)socket.get())->NativeUdpFd();
    int fd = pUdp ? *pUdp : -1;
    DebugPrint(dbg_api, "sock = %d, return udp fd = %d", sock, fd);
    return fd;
}

uint64_t GetQuicConnectionId(QuicSocket sock)
{
    auto socket = EntryBase::GetFdManager().Get(sock);
    if (!socket || socket->Category() != EntryCategory::Socket) {
        DebugPrint(dbg_api, "sock = %d, return = -1", sock);
        return -1;
    }

    uint64_t id = ((QuicSocketEntry*)socket.get())->connection_id();
    DebugPrint(dbg_api, "sock = %d, return connection_id = %lu", sock, id);
    return id;
}

} // namespace posix_quic
