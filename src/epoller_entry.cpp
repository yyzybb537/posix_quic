#include "epoller_entry.h"
#include "clock.h"
#include "socket_entry.h"
#include "stream_entry.h"
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>

namespace posix_quic {


void QuicEpollerEntry::EpollTrigger::OnTrigger(short int event)
{
    // mod fd
    epollEntry->Notify();
}

void QuicEpollerEntry::Notify()
{
    if (notifyProtect_.test_and_set()) {
        DebugPrint(dbg_event, "Fake Notify epfd = %d", Fd());
        return ;
    }

    struct epoll_event ev;
    ev.events = EPOLLOUT | EPOLLET;
    ev.data.fd = socketPair_[0];
    ::epoll_ctl(Fd(), EPOLL_CTL_MOD, socketPair_[0], &ev);

    DebugPrint(dbg_event, "Notify epfd = %d", Fd());
}

void QuicEpollerEntry::EpollTrigger::OnClose(Event* e)
{
    epollEntry->Del(e->Fd());
}

QuicEpollerEntry::QuicEpollerEntry()
{
    SetFd(epoll_create(10240));
    trigger_.epollfd = Fd();
    trigger_.epollEntry = this;
    signal(EPIPE, SIG_IGN);

    int res = socketpair(AF_LOCAL, SOCK_STREAM, 0, socketPair_);
    if (res != 0) {
        DebugPrint(dbg_error, "socketpair returns %d, errno = %d", res, errno);
        socketPair_[0] = socketPair_[1] = -1;
    } else {
        struct epoll_event ev;
        ev.events = EPOLLOUT | EPOLLET;
        ev.data.fd = socketPair_[0];
        ::epoll_ctl(Fd(), EPOLL_CTL_ADD, socketPair_[0], &ev);
    }
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

    for (auto pairFd : socketPair_)
        if (pairFd != -1)
            ::close(pairFd);
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
    int res = AddInner(fd, event);
    DebugPrint(dbg_epoll, "fd = %d, events = %s", fd, EpollEvent2Str(event->events));
    return res;
}
int QuicEpollerEntry::AddInner(int fd, struct epoll_event * event)
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

    std::shared_ptr<quic_epoll_event> qev(new quic_epoll_event);
    qev->events = Epoll2Poll(event->events);
    qev->data = event->data;
    qev->revents = 0;

    Event::EventWaiter waiter = { &qev->events, &qev->revents };
    if (!entry->StartWait(waiter, &trigger_)) {
        errno = EBADF;
        return -1;
    }

    fds_[fd] = std::make_pair(EntryWeakPtr(entry), qev);

    // listen udp
    std::shared_ptr<int> udpSocket = entry->NativeUdpFd();
    if (!udpSocket) {
        // QuicSocket必须已经有了对应的udp socket才能加入Epoller.
        // 自行创建的QuicSocket, Bind后就可以了.
        entry->StopWait(&trigger_);
        errno = EINVAL;
        return -1;
    }

    if (entry->Category() == EntryCategory::Socket) {
        if (!((QuicSocketEntry*)entry.get())->GetQuicTaskRunnerProxy()->Link(&taskRunner_)) {
            entry->StopWait(&trigger_);
            errno = EBUSY;
            return -1;
        }
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

    return 0;
}
int QuicEpollerEntry::Mod(int fd, struct epoll_event * event)
{
    int res = ModInner(fd, event);
    DebugPrint(dbg_epoll, "fd = %d, events = %s, return %d", fd, EpollEvent2Str(event->events), res);
    return res;
}
int QuicEpollerEntry::ModInner(int fd, struct epoll_event * event)
{
    std::unique_lock<std::mutex> lock(mtx_);
    auto itr = fds_.find(fd);
    if (itr == fds_.end()) {
        errno = ENOENT;
        return -1;
    }

    int pollevent = Epoll2Poll(event->events);

    auto & qev = itr->second.second;
    qev->events = pollevent;
    qev->data = event->data;
    if (pollevent & POLLOUT) {
        __atomic_fetch_or(&qev->revents, POLLOUT, std::memory_order_seq_cst);
    }
    Notify();
    DebugPrint(dbg_epoll, "fd = %d, revents = %s", fd, PollEvent2Str(qev->revents));
    return 0;
}
int QuicEpollerEntry::Del(int fd)
{
    int res = DelInner(fd);
    DebugPrint(dbg_epoll, "fd = %d", fd);
    return res;
}
int QuicEpollerEntry::DelInner(int fd)
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
    assert(udpItr != udps_.end());
    if (--udpItr->second == 0) {
        epoll_ctl(Fd(), EPOLL_CTL_DEL, *udpSocket, nullptr);
        udps_.erase(udpItr);
    }

    entry->StopWait(&trigger_);
    fds_.erase(itr);

    if (entry->Category() == EntryCategory::Socket) {
        ((QuicSocketEntry*)entry.get())->GetQuicTaskRunnerProxy()->Unlink();
    }

    return 0;
}

int QuicEpollerEntry::Wait(struct epoll_event *events, int maxevents, int timeout)
{
    DebugPrint(dbg_epoll, "QuicEpollerEntry::Wait begin");

    size_t udpSize = std::max<size_t>(udps_.size() + 1, 1);
    udpEvents_.resize(std::min<size_t>(udpSize, 10240));
    udpRecvBuf_.resize(65 * 1024);
    
    int res = Poll(events, maxevents);
    if (res > 0) {
        DebugPrint(dbg_epoll, "QuicEpollerEntry::Wait poll success. returns %d", res);
        return res;
    }

    int64_t deadline = QuicClockImpl::getInstance().NowMS() + timeout;

    for (;;) {
        taskRunner_.RunOnce();

        int64_t now = QuicClockImpl::getInstance().NowMS();
        int64_t timeRemain = deadline > now ? deadline - now : 0;
        int64_t onceTimeout = std::min<int64_t>(timeRemain, 10);

retry_wait:
        res = epoll_wait(Fd(), &udpEvents_[0], udpEvents_.size(), onceTimeout);
        if (res < 0) {
            if (errno == EINTR)
                goto retry_wait;

            return res;
        }

        if (res > 0) break;
        if (res == 0 && (onceTimeout == 0 || timeRemain == onceTimeout)) break;
    }

    // clear notify flag
    notifyProtect_.clear();

    for (int i = 0; i < res; ++i) {
        struct epoll_event & ev = udpEvents_[i];
        int udpFd = ev.data.fd;
        if (ev.data.fd == socketPair_[0])
            continue;

        if (ev.events & EPOLLIN == 0)
            continue;

        QuicSocketEntryPtr owner;
        QuicSocketAddress selfAddress(GetLocalAddress(udpFd));

        for (int j = 0; j < 10240; ++j) {
            struct sockaddr_storage addr = {};
            socklen_t addrLen = sizeof(addr);
retry_recvfrom:
            int bytes = ::recvfrom(udpFd, &udpRecvBuf_[0], udpRecvBuf_.size(),
                    0, reinterpret_cast<struct sockaddr*>(&addr), &addrLen);
            ErrnoStore es;
            QuicSocketAddress peerAddress(addr);
            DebugPrint(dbg_epoll | dbg_read, "syscall -> recvfrom %s. Udp socket = %d, bytes = %d, errno = %d",
                    peerAddress.ToString().c_str(),
                    udpFd, bytes, errno);
            es.Restore();
            if (bytes < 0) {
                if (errno == EINTR)
                    goto retry_recvfrom;
                break;
            }

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
                socket->OnSyn(owner, peerAddress);
                socket->ProcessUdpPacket(selfAddress,
                        peerAddress,
                        QuicReceivedPacket(&udpRecvBuf_[0], bytes, QuicClockImpl::getInstance().Now()));
                continue;
            }

            DebugPrint(dbg_epoll | dbg_read, " -> recvfrom. Udp socket = %d, connectionId = %lu is exists.", udpFd, connectionId);


            EntryPtr entry = QuicSocketEntry::GetFdManager().Get(quicSocket);
            assert(entry->Category() == EntryCategory::Socket);

            QuicSocketEntry* socket = (QuicSocketEntry*)entry.get();
            socket->ProcessUdpPacket(selfAddress,
                peerAddress,
                QuicReceivedPacket(&udpRecvBuf_[0], bytes, QuicClockImpl::getInstance().Now()));
        }
    }

    res = Poll(events, maxevents);
    DebugPrint(dbg_epoll, "QuicEpollerEntry::Wait returns %d", res);
    return res;
}
int QuicEpollerEntry::Poll(struct epoll_event *events, int maxevents)
{
    std::unique_lock<std::mutex> lock(mtx_);
    int i = 0;
    for (auto & kv : fds_) {
        if (i >= maxevents) break;

        quic_epoll_event & qev = *(kv.second.second);
        short int event = qev.events | POLLERR;

//        DebugPrint(dbg_event, "fd = %d, qev.revents = %s, event = %s",
//                kv.first, PollEvent2Str(qev.revents), PollEvent2Str(event));

        short int revents = __atomic_fetch_and(&qev.revents, ~event, std::memory_order_seq_cst);
        revents &= event;

        DebugPrint(dbg_event, "after __atomic_fetch_and fd = %d, qev.revents = %s, revents = %s",
                kv.first, PollEvent2Str(qev.revents), PollEvent2Str(revents));

        if (revents == 0) continue;

        struct epoll_event & ev = events[i++];
        ev.data = qev.data;
        ev.events = Poll2Epoll(revents);
    }

    DebugPrint(dbg_epoll, "QuicEpollerEntry::Poll returns %d", i);
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
    socklen_t addrLen = sizeof(addr);
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

std::string QuicEpollerEntry::GetDebugInfo(int indent)
{
    std::string idt(indent, ' ');
    std::string idt2 = idt + ' ';
    std::string idt3 = idt2 + ' ';

    std::unique_lock<std::mutex> lock(mtx_);
    std::string info;
    info += idt + P("=========== epoll: %d ===========", Fd());

    std::string socketInfo, streamInfo, closedInfo;
    int socketCount = 0, streamCount = 0, closedCount = 0;
    for (auto & kv : fds_) {
        int fd = kv.first;
        EntryPtr entry = kv.second.first.lock();
        quic_epoll_event & qev = *(kv.second.second);
        UdpSocket udp = -1;
        if (entry) {
            auto udpSocket = entry->NativeUdpFd();
            if (udpSocket)
                udp = *udpSocket;
        }
        std::string fdInfo(idt3);
        fdInfo += P("-> fd: %d, UdpSocket: %d, events: %s, revents: %s",
                fd, udp, EpollEvent2Str(qev.events), EpollEvent2Str(qev.revents));
        if (!entry) {
            closedInfo += fdInfo;
            closedCount++;
        } else if (entry->Category() == EntryCategory::Socket) {
            socketInfo += fdInfo;
            socketCount++;
        } else if (entry->Category() == EntryCategory::Stream) {
            streamInfo += fdInfo;
            streamCount++;
        }
    }

    info += idt2 + P("------------ QuicSocket (count=%d) ------------", socketCount);
    info += socketInfo;

    info += idt2 + P("------------ QuicStream (count=%d) ------------", streamCount);
    info += streamInfo;

    info += idt2 + P("------------ Closed (count=%d) ------------", closedCount);
    info += closedInfo;

    info += idt2 + P("------------ UDP (count=%d) ------------", (int)udps_.size());
    for (auto & kv : udps_) {
        int udpSocket = kv.first;
        long ref = kv.second;
        info += idt3 + P("-> udp: %d, refcount: %ld", udpSocket, ref);
    }
    info += idt + P("======================================");
    return info;
}

} // namespace posix_quic
