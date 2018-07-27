#pragma once

#include <unordered_map>
#include <mutex>
#include "entry.h"

namespace posix_quic {

struct QuicEpollerEntry : public EntryBase
{
public:
    typedef uint32_t ListenEvent;
    typedef std::unordered_map<
                int,
                std::pair<EntryWeakPtr, ListenEvent>
            > FdContainer;

private:
    std::mutex mtx_;
    int epollfd_;
    FdContainer fds_;
};

} // namespace posix_quic
