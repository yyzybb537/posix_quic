#include "packet_transport.h"

namespace posix_quic {

void PosixQuicPacketTransport::Set(std::shared_ptr<int> udpSocket, QuicSocketAddress const& address)
{
    udpSocket_ = udpSocket;
    address_ = address;
}

int PosixQuicPacketTransport::Write(const char* buffer, size_t buf_len)
{
    if (!udpSocket_) {
        DebugPrint(dbg_write, "buffer length = %d, no udp socket. return = -1", (int)buf_len);
        return -1;
    }

    if (!address_.IsInitialized()) {
        DebugPrint(dbg_write, "Udp socket = %d, buffer length = %d, address not initialized. return = -1", *udpSocket_, (int)buf_len);
        return -1;
    }

    struct sockaddr_storage addr = address_.generic_address();
    int res = ::sendto(*udpSocket_, buffer, buf_len, 0, (const struct sockaddr*)&addr, sizeof(addr));
    DebugPrint(dbg_write, "Udp socket = %d, buffer length = %d, return = %d, errno = %d",
            *udpSocket_, (int)buf_len, res, errno);
    return res;
}

} // namespace posix_quic
