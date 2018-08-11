#pragma once

#include "simple_fwd.h"
#include "quic_socket.h"
#include "connection.h"

namespace posix_quic {
namespace simple {

class IOService
{
public:
    IOService();

    QuicEpoller Native() const { return ep_; }

    void Run(int timeout = 0);

    void RunLoop();

private:
    QuicEpoller ep_;
};

} // amespace simple
} // namespace posix_quic
