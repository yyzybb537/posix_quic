#pragma once

#include <unordered_map>
#include <mutex>
#include "entry.h"
#include <sys/epoll.h>

namespace posix_quic {

class QuicEpollerEntry : public EntryBase
{
public:
    struct quic_epoll_event : public epoll_event {
        uint32_t revents;
    };

    typedef std::unordered_map<
                int,
                std::pair<EntryWeakPtr, std::shared_ptr<quic_epoll_event>>
            > FdContainer;

    // 引用计数
    typedef std::unordered_map<int, long> UdpContainer;

    QuicEpollerEntry();
    ~QuicEpollerEntry();

    static QuicEpollerEntryPtr NewQuicEpollerEntry();

    int Add(int fd, int events, struct epoll_event *event);

    int Mod(int fd, int events, struct epoll_event *event);

    int Del(int fd, int events);

    int Wait(struct epoll_event *events, int maxevents, int timeout);

protected:
    FdManager<QuicEpollerEntryPtr> & GetFdManager();

    QuicSocketAddress GetLocalAddress(UdpSocket udpSocket);

    QuicSocketAddress MakeAddress(struct sockaddr_in* addr, socklen_t addrLen);

private:
    std::mutex mtx_;
    FdContainer fds_;
    UdpContainer udps_;
    std::vector<struct epoll_event> udpEvents_;
    std::vector<char> udpRecvBuf_;
    Event::EventTrigger trigger_;
};

} // namespace posix_quic
