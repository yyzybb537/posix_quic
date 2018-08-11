#include "session.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include "clock.h"
#include "task_runner.h"
#include "stream_entry.h"
#include "socket_entry.h"

namespace posix_quic {

using namespace net;

void QuicSocketSession::Bind(QuicSocketEntry * wrap, std::recursive_mutex * mtx)
{
    wrap_ = wrap;
    mtx_ = mtx;
}

void QuicSocketSession::ProcessUdpPacket(const QuicSocketAddress& self_address,
        const QuicSocketAddress& peer_address,
        const QuicReceivedPacket& packet)
{
    QuartcSession::ProcessUdpPacket(self_address, peer_address, packet);
}

} // namespace posix_quic
