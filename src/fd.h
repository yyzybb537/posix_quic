#pragma once

#include "fwd.h"

namespace posix_quic {

class FdBase
{
public:
    virtual ~FdBase() {}

    void SetFd(int fd) { fd_ = fd; }
    int Fd() const { return fd_; }

private:
    int fd_ = 0;
};

} // namespace posix_quic

