#include "epoller_entry.h"
#include "clock.h"
#include "socket_entry.h"
#include "stream_entry.h"
#include <sys/socket.h>
#include <poll.h>

namespace posix_quic {

QuicEpollerEntry::QuicEpollerEntry()
{
    SetFd(epoll_create(10240));
}

QuicEpollerEntry::~QuicEpollerEntry()
{
    std::unique_lock<std::mutex> lock(mtx_);
    ::close(Fd());
    SetFd(-1);
    udps_.clear();
    for (auto & kv : fds_) {
        EntryPtr entry = kv.second.first.lock();
        if (!entry) continue ;

        entry->StopWait(&trigger_);
    }
    fds_.clear();
}

QuicEpollerEntryPtr QuicEpollerEntry::NewQuicEpollerEntry()
{
    QuicEpollerEntryPtr ptr(new QuicEpollerEntry);
    if (ptr->Fd() >= 0) {
        GetFdManager().Put(ptr->Fd(), ptr);
        return ptr;
    }
    return QuicEpollerEntryPtr();
}
void QuicEpollerEntry::DeleteQuicEpollerEntry(QuicEpollerEntryPtr ep)
{
    GetFdManager().Delete(ep->Fd());
}
int QuicEpollerEntry::Add(int fd, struct epoll_event * event)
{
    std::unique_lock<std::mutex> lock(mtx_);
    auto itr = fds_.find(fd);
    if (itr != fds_.end()) {
        EntryPtr entry = itr->second.first.lock();
        if (entry && entry->Fd() == fd) {
            errno = EEXIST;
            return -1;
        }

        fds_.erase(itr);
    }

    EntryPtr entry = EntryBase::GetFdManager().Get(fd);
    if (!entry || entry->Fd() != fd) {
        errno = EBADF;
        return -1;
    }

    std::shared_ptr<int> udpSocket = entry->NativeUdpFd();
    if (!udpSocket) {
        // QuicSocket必须已经有了对应的udp socket才能加入Epoller.
        // 自行创建的QuicSocket, Bind后就可以了.
        errno = EINVAL;
        return -1;
    }

    auto udpItr = udps_.find(*udpSocket);
    if (udps_.end() == udpItr) {
        struct epoll_event udpEv;
        udpEv.events = EPOLLIN;
        udpEv.data.fd = *udpSocket;
        int res = epoll_ctl(Fd(), EPOLL_CTL_ADD, *udpSocket, &udpEv);
        if (res < 0) {
            return -1;
        }
        udps_[*udpSocket] = 1;

        DebugPrint(dbg_epoll, "Add udp socket = %d, quicFd = %d", *udpSocket, fd);
    } else {
        udpItr->second++;
        DebugPrint(dbg_epoll, "Ref udp socket = %d, quicFd = %d", *udpSocket, fd);
    }

    std::shared_ptr<quic_epoll_event> qev(new quic_epoll_event);
    qev->events = Epoll2Poll(event->events);
    qev->data = event->data;
    qev->revents = 0;

    fds_[fd] = std::make_pair(EntryWeakPtr(entry), qev);

    Event::EventWaiter waiter = { &qev->events, &qev->revents };
    entry->StartWait(waiter, &trigger_);
    return 0;
}
int QuicEpollerEntry::Mod(int fd, struct epoll_event * event)
{
    std::unique_lock<std::mutex> lock(mtx_);
    auto itr = fds_.find(fd);
    if (itr == fds_.end()) {
        errno = ENOENT;
        return -1;
    }

    auto & qev = itr->second.second;
    qev->events = Epoll2Poll(event->events);
    qev->data = event->data;
    return 0;
}
int QuicEpollerEntry::Del(int fd)
{
    std::unique_lock<std::mutex> lock(mtx_);
    auto itr = fds_.find(fd);
    if (itr == fds_.end()) {
        errno = ENOENT;
        return -1;
    }

    EntryPtr entry = itr->second.first.lock();
    if (!entry || entry->Fd() != fd) {
        fds_.erase(itr);
        errno = ENOENT;
        return -1;
    }

    std::shared_ptr<int> udpSocket = entry->NativeUdpFd();

    // UdpSocket在QuicSocket/QuicStream中是不可改变的
    assert(!!udpSocket);

    auto udpItr = udps_.find(*udpSocket);
    if (--udpItr->second == 0) {
        epoll_ctl(Fd(), EPOLL_CTL_DEL, *udpSocket, nullptr);
        udps_.erase(udpItr);
    }

    entry->StopWait(&trigger_);
    fds_.erase(itr);
    return 0;
}
int QuicEpollerEntry::Wait(struct epoll_event *events, int maxevents, int timeout)
{
    udpEvents_.resize((std::max<size_t>)(udps_.size(), 1));
    udpRecvBuf_.resize(65 * 1024);

    int res = epoll_wait(Fd(), &udpEvents_[0], udpEvents_.size(), timeout);
    if (res < 0) {
        return res;
    }

    for (int i = 0; i < res; ++i) {
        struct epoll_event & ev = udpEvents_[i];
        int udpFd = ev.data.fd;
        if (ev.events & EPOLLIN == 0)
            continue;

        QuicSocketEntryPtr owner;

        for (int j = 0; j < 1024; ++j) {
            struct sockaddr_storage addr = {};
            socklen_t addrLen = sizeof(addr);
            int bytes = ::recvfrom(udpFd, &udpRecvBuf_[0], udpRecvBuf_.size(), 0, (struct sockaddr*)&addr, &addrLen);
            DebugPrint(dbg_epoll | dbg_read, "syscall -> recvfrom. Udp socket = %d, bytes = %d, errno = %d", udpFd, bytes, errno);
            if (bytes < 0) break;

            // 1.解析quic framer
            QuicConnectionId connectionId = headerParser_.ParseHeader(&udpRecvBuf_[0], bytes);
            if (connectionId == INVALID_QUIC_CONNECTION_ID) {
                // 不是quic包, 直接丢弃
                DebugPrint(dbg_ignore, " -> recvfrom. Udp socket = %d, recv not quic packet. Hex = [%s]",
                        udpFd, Bin2Hex(&udpRecvBuf_[0], (std::min)(bytes, 9)).c_str());
                continue;
            }

            // 2.根据ConnectionId来分派, 给指定的QuicSocket
            QuicSocket quicSocket = QuicSocketEntry::GetConnectionManager().Get(udpFd, connectionId);
            if (quicSocket == -1) {

                // 新连接
                if (!owner) {
                    QuicSocket ownerFd = QuicSocketEntry::GetConnectionManager().GetOwner(udpFd);
                    if (ownerFd >= 0) {
                        EntryPtr ownerBase = QuicSocketEntry::GetFdManager().Get(ownerFd);
                        assert(ownerBase->Category() == EntryCategory::Socket);
                        owner = std::dynamic_pointer_cast<QuicSocketEntry>(ownerBase);
                    }
                }

                if (!owner) {
                    // listen socket已关闭, 不再接受新连接
                    DebugPrint(dbg_epoll | dbg_read | dbg_accept,
                            " -> recvfrom. Udp socket = %d, connectionId = %lu is a new Connection. Not found owner, so ignore it!",
                            udpFd, connectionId);
                    continue;
                }

                DebugPrint(dbg_epoll | dbg_read | dbg_accept,
                        " -> recvfrom. Udp socket = %d, connectionId = %lu is a new Connection.",
                        udpFd, connectionId);

                QuicSocketEntryPtr socket = QuicSocketEntry::NewQuicSocketEntry(true, connectionId);
                socket->OnSyn(owner, QuicSocketAddress(addr));
                socket->StartCryptoHandshake();
                socket->ProcessUdpPacket(GetLocalAddress(udpFd),
                        QuicSocketAddress(addr),
                        QuicReceivedPacket(&udpRecvBuf_[0], bytes, QuicClockImpl::getInstance().Now()));
                continue;
            }

            DebugPrint(dbg_epoll | dbg_read, " -> recvfrom. Udp socket = %d, connectionId = %lu is exists.", udpFd, connectionId);


            EntryPtr entry = QuicSocketEntry::GetFdManager().Get(quicSocket);
            assert(entry->Category() == EntryCategory::Socket);

            QuicSocketEntry* socket = (QuicSocketEntry*)entry.get();
            socket->FlushWrites();
            socket->ProcessUdpPacket(GetLocalAddress(udpFd),
                QuicSocketAddress(addr),
                QuicReceivedPacket(&udpRecvBuf_[0], bytes, QuicClockImpl::getInstance().Now()));
        }
    }

    std::unique_lock<std::mutex> lock(mtx_);
    int i = 0;
    for (auto & kv : fds_) {
        if (i >= maxevents) break;

        quic_epoll_event & qev = *(kv.second.second);
        short int event = qev.events;

        short int revents = __atomic_and_fetch(&qev.revents, ~event, std::memory_order_seq_cst);
        revents &= event;

        if (revents == 0) continue;

        struct epoll_event & ev = events[i++];
        ev.data = qev.data;
        ev.events = Poll2Epoll(revents);
    }

    return i;
}
FdManager<QuicEpollerEntryPtr> & QuicEpollerEntry::GetFdManager()
{
    static FdManager<QuicEpollerEntryPtr> obj;
    return obj;
}

QuicSocketAddress QuicEpollerEntry::GetLocalAddress(UdpSocket udpSocket)
{
    struct sockaddr_storage addr = {};
    socklen_t addrLen;
    int res = getsockname(udpSocket, (struct sockaddr*)&addr, &addrLen); 
    if (res != 0) return QuicSocketAddress();
    return QuicSocketAddress(addr);
}

short int QuicEpollerEntry::Epoll2Poll(uint32_t event)
{
    short int ev = 0;
    if (event & EPOLLIN)
        ev |= POLLIN;
    if (event & EPOLLOUT)
        ev |= POLLOUT;
    if (event & EPOLLERR)
        ev |= POLLERR;
    return ev;
}

uint32_t QuicEpollerEntry::Poll2Epoll(short int event)
{
    uint32_t ev = 0;
    if (event & POLLIN)
        ev |= EPOLLIN;
    if (event & POLLOUT)
        ev |= EPOLLOUT;
    if (event & POLLERR)
        ev |= EPOLLERR;
    return ev;
}

} // namespace posix_quic
