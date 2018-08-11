#pragma once

#include "fwd.h"
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <list>
#include "entry.h"
#include "packet_transport.h"
#include "connection_manager.h"
#include "net/quic/quartc/quartc_session_interface.h"
#include "connection_visitor.h"
#include "task_runner.h"

namespace posix_quic {

class QuicSocketEntry;

class QuicSocketSession
    : public QuartcSession
{
public:
    using QuartcSession::QuartcSession;

    virtual ~QuicSocketSession() {}

    void Bind(QuicSocketEntry * wrap, std::recursive_mutex * mtx);

    // --------------------------------
    // Called in epoll trigger loop
    void ProcessUdpPacket(const QuicSocketAddress& self_address,
            const QuicSocketAddress& peer_address,
            const QuicReceivedPacket& packet) override;
    // --------------------------------

private:
    QuicSocketEntry * wrap_;
    std::recursive_mutex * mtx_;
};

} // namespace posix_quic
