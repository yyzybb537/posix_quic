#include "header_parser.h"
#include "clock.h"

namespace posix_quic {

QuicConnectionId HeaderParser::ParseHeader(const char* data, size_t len)
{
    if (len < 9) return INVALID_QUIC_CONNECTION_ID;
    unsigned char flag = *(unsigned char*)data;

    if (flag & 0x08 == 0) return INVALID_QUIC_CONNECTION_ID;

    // 保留字段, 如果更高版本使用到了, 需要修改此处校验
    if (flag & 0x80 != 0) return INVALID_QUIC_CONNECTION_ID;

    // QUIC协议使用的是小尾, 而不是网络字节序.
    QuicConnectionId id = *(QuicConnectionId*)(data + 1);
    return id;
}

} // namespace posix_quic
