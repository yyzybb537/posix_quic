#include "epoller_entry.h"
#include "clock.h"

namespace posix_quic {

QuicEpollerEntry::QuicEpollerEntry()
{
    SetFd(epoll_create(10240));
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
int QuicEpollerEntry::Add(int fd, int events, struct epoll_event * event)
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
    } else {
        udpItr->second++;
    }

    std::shared_ptr<quic_epoll_event> qev(new quic_epoll_event);
    qev->events = event->events;
    qev->data = event->data;
    qev->revents = 0;

    fds_[fd] = std::make_pair(EntryWeakPtr(entry), qev);

    Event::EventWaiter waiter = { &qev->events, &qev->revents };
    entry->StartWait(waiter, &trigger_);
    return 0;
}
int QuicEpollerEntry::Mod(int fd, int events, struct epoll_event * event)
{
    std::unique_lock<std::mutex> lock(mtx_);
    auto itr = fds_.find(fd);
    if (itr == fds_.end()) {
        errno = ENOENT;
        return -1;
    }

    auto & qev = itr->second.second;
    qev->events = event->events;
    qev->data = event->data;
    return 0;
}
int QuicEpollerEntry::Del(int fd, int events)
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
            struct sockaddr_in addr;
            socklen_t addrLen;
            int bytes = ::recvfrom(udpFd, &udpRecvBuf_[0], udpRecvBuf_.size(), 0, (struct sockaddr*)&addr, &addrLen);
            if (bytes < 0) break;

            // 1.解析quic framer

            // 2.根据ConnectionId来分派, 给指定的QuicSocket
            QuicSocket quicSocket = QuicSocketEntry::GetConnectionManager().Get(udpFd, connectionId);
            if (quicSocket == -1) {
                // 新连接
                if (!owner) {
                    QuicSocket ownerFd = QuicSocketEntry::GetConnectionManager().GetOwner(udpFd);
                    if (ownerFd >= 0)
                        owner = QuicSocketEntry::GetFdManager().Get(ownerFd);
                }

                if (!owner) {
                    // listen socket已关闭, 不再接受新连接
                    continue;
                }

                QuicSocketEntryPtr socket = QuicSocketEntry::NewQuicSocketEntry();
                socket->OnSyn(onwer);
                socket->ProcessUdpPacket(GetLocalAddress(udpFd), MakeAddress(&addr, addrLen), QuicClockImpl::getInstance().Now());
                socket->StartCryptoHandshake();
                continue;
            }

            QuicSocketEntryPtr socket = QuicSocketEntry::GetFdManager().Get(quicSocket);
            socket->ProcessUdpPacket(GetLocalAddress(udpFd), MakeAddress(&addr, addrLen), QuicClockImpl::getInstance().Now());
        }
    }
}
FdManager<QuicEpollerEntryPtr> & QuicEpollerEntry::GetFdManager()
{
    static FdManager<QuicEpollerEntryPtr> obj;
    return obj;
}

} // namespace posix_quic
