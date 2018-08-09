#pragma once

#include "net/quic/core/quic_constants.h"

namespace posix_quic {

const int kDefaultAckTimeout = 12;

const int kDefaultIdleTimeout = net::kPingTimeoutSecs + 13;

const int kMinIdleTimeout = 1;

} // namespace posix_quic
