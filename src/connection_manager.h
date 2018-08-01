#pragma once

#include "fwd.h"
#include <unordered_map>
#include <mutex>

namespace posix_quic {

struct ConnectionManager
{
public:
    typedef std::unordered_map<QuicConnectionId, QuicSocket> UdpConnectionMap;
    typedef std::unordered_map<UdpSocket, UdpConnectionMap> ConnectionMap;
    typedef std::unordered_map<UdpSocket, QuicSocket> OwnerMap;

    void Put(UdpSocket udpSocket, QuicConnectionId connectionId, QuicSocket quicSocket, bool owner) {
        std::unique_lock<std::mutex> lock(mtx_);
        connections_[udpSocket][connectionId] = quicSocket;
        if (owner) {
            owners_[udpSocket] = quicSocket;
        }
    }

    QuicSocket Get(UdpSocket udpSocket, QuicConnectionId connectionId) {
        std::unique_lock<std::mutex> lock(mtx_);
        auto it1 = connections_.find(udpSocket);
        if (connections_.end() == it1) return -1;

        auto it2 = it1->second.find(connectionId);
        if (it1->second.end() == it2) return -1;

        return it2->second;
    }

    QuicSocket GetOwner(UdpSocket udpSocket) {
        std::unique_lock<std::mutex> lock(mtx_);
        auto it = owners_.find(udpSocket);
        if (it != owners_.end())
            return it->second;

        return -1;
    }

    bool Delete(UdpSocket udpSocket, QuicConnectionId connectionId, QuicSocket quicSocket) {
        std::unique_lock<std::mutex> lock(mtx_);
        auto it1 = connections_.find(udpSocket);
        if (connections_.end() == it1) return false;

        auto it2 = it1->second.find(connectionId);
        if (it1->second.end() == it2) return false;

        it1->second.erase(it2);
        if (it1->second.empty())
            connections_.erase(it1);

        auto it = owners_.find(udpSocket);
        if (it != owners_.end() && it->second == quicSocket) {
            owners_.erase(it);
        }
        return true;
    }

private:
    std::mutex mtx_;
    ConnectionMap connections_;
    OwnerMap owners_;
};

} // namespace posix_quic
