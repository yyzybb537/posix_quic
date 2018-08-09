#pragma once

namespace posix_quic {

const int kDefaultAckTimeout = 12;

// net::kPingTimeoutSecs == 15
const int kDefaultIdleTimeout = 15 + 13;

const int kMinIdleTimeout = 1;

} // namespace posix_quic
