#include "packet_transport.h"

namespace posix_quic {

void Set(std::shared_ptr<int> udpSocket, QuicSocketAddress const& address)
{
    udpSocket_ = udpSocket;
    address_ = address;
}

int Write(const char* buffer, size_t buf_len)
{
    if (!udpSocket || !address_.IsInitialized()) {
        return -1;
    }

    auto s = address_.generic_address();
    return ::sendto(*udpSocket_, buffer, buf_len, 0, (const struct sockaddr*)&s, sizeof(struct sockaddr_in));
}

} // namespace posix_quic
