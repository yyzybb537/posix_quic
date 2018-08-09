#include "packet_transport.h"

namespace posix_quic {

void PosixQuicPacketTransport::Set(std::shared_ptr<int> udpSocket, QuicSocketAddress const& address)
{
    udpSocket_ = udpSocket;
    address_ = address;
}

void PosixQuicPacketTransport::UpdatePeerAddress(QuicSocketAddress const& address)
{
    DebugPrint(dbg_write, "UpdatePeerAddress from %s to %s", address_.ToString().c_str(), address.ToString().c_str());
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
retry_sendto:
    int res = ::sendto(*udpSocket_, buffer, buf_len, 0, (const struct sockaddr*)&addr, sizeof(addr));
    if (res == -1 && errno == EINTR)
        goto retry_sendto;

    DebugPrint(dbg_write, "sento %s Udp socket = %d, buffer length = %d, return = %d, errno = %d",
            address_.ToString().c_str(), *udpSocket_, (int)buf_len, res, errno);

    // 告诉协议栈, udp总是可用的, 以便于切网也可以正常重传
    return buf_len;
}

} // namespace posix_quic
