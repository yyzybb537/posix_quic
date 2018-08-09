#pragma once

#include <string.h>

namespace posix_quic {

enum eQuicSocketOptionType
{
    // 需要对端回复的时候, N秒没有收到回复或任何有效包, 即认为链接断开
    sockopt_ack_timeout_secs,

    // 链路空闲超时, 由于客户端会发心跳, 这个可以用于server端检测链接是否断开
    sockopt_idle_timeout_secs,

    sockopt_count,
};

template <int Count>
class Options
{
public:
    Options() {
        memset(values_, 0, sizeof(values_));
    }
    
    // @return: Was changed ?
    bool SetOption(int type, long value) {
        if (type >= Count) return false;

        if (values_[type] != value) {
            values_[type] = value;
            return true;
        }

        return false;
    }

    long GetOption(int type) const {
        if (type >= Count) return 0;
        return values_[type];
    }

private:
    long values_[Count];
};

typedef Options<sockopt_count> QuicSocketOptions;

} // namespace posix_quic
