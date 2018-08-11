#pragma once

#include "simple_fwd.h"
#include "quic_socket.h"
#include "connection.h"
#include <unordered_map>

namespace posix_quic {
namespace simple {

class IOService
{
public:
    IOService();

    QuicEpoller Native() const { return ep_; }

    void Run(int timeout = 0);

    void RunLoop();

    uint64_t AddContext(Connection::Context* ctx);

    void DelContext(uint64_t ctxId);

private:
    Connection::Context* GetContext(uint64_t ctxId);

private:
    QuicEpoller ep_;

    std::mutex contextTableMtx_;

    std::unordered_map<uint64_t, Connection::Context*> contextTable_;

    uint64_t ctxIdInc_;
};

} // amespace simple
} // namespace posix_quic
