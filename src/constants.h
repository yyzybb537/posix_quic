#pragma once

namespace posix_quic {

// 需要对端回复的时候, N秒没有收到回复或任何有效包, 即认为链接断开
const int kDefaultAckTimeout = 12;

// 链路空闲超时, 由于客户端会发心跳, 这个可以用于server端检测链接是否断开
// net::kPingTimeoutSecs == 15
const int kDefaultIdleTimeout = 15 + 13;

const int kMinIdleTimeout = 1;

// 底层udp socket的收发缓冲区大小 (默认设置为5M)
const int kDefaultUdpRecvMem = 5 * 1024 * 1024;

const int kDefaultUdpWriteMem = 5 * 1024 * 1024;

} // namespace posix_quic
