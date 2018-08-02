#pragma once

#include "fwd.h"
#include <unordered_map>
#include <mutex>
#include "entry.h"
#include <sys/epoll.h>
#include "header_parser.h"

namespace posix_quic {

class QuicEpollerEntry : public Event
{
public:
    struct quic_epoll_event {
        short int events;
        short int revents;
        epoll_data_t data;
    };

    typedef std::unordered_map<
                int,
                std::pair<EntryWeakPtr, std::shared_ptr<quic_epoll_event>>
            > FdContainer;

    // 引用计数
    typedef std::unordered_map<int, long> UdpContainer;

    QuicEpollerEntry();
    ~QuicEpollerEntry();

    static FdManager<QuicEpollerEntryPtr> & GetFdManager();

    static QuicEpollerEntryPtr NewQuicEpollerEntry();

    static void DeleteQuicEpollerEntry(QuicEpollerEntryPtr ep);

    int Add(int fd, struct epoll_event *event);

    int Mod(int fd, struct epoll_event *event);

    int Del(int fd);

    int Wait(struct epoll_event *events, int maxevents, int timeout);

    const char* DebugTypeInfo() override { return "Epoll"; };

protected:
    QuicSocketAddress GetLocalAddress(UdpSocket udpSocket);

    short int Epoll2Poll(uint32_t event);

    uint32_t Poll2Epoll(short int event);

    int Poll(struct epoll_event *events, int maxevents);

private:
    std::mutex mtx_;
    FdContainer fds_;
    UdpContainer udps_;
    std::vector<struct epoll_event> udpEvents_;
    std::vector<char> udpRecvBuf_;
    Event::EventTrigger trigger_;
    HeaderParser headerParser_;
};

} // namespace posix_quic
