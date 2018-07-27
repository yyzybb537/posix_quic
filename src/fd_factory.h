#pragma once

#include <queue>
#include <mutex>

namespace posix_quic {

struct FdFactory
{
public:
    int Alloc() {
        std::unique_lock<std::mutex> lock(mtx_);
        if (free_.empty()) return ++acq_;
        int fd = free_.front();
        free_.pop();
        return fd;
    }

    void Free(int fd) {
        std::unique_lock<std::mutex> lock(mtx_);
        free_.push(fd);
    }

private:
    std::mutex mtx_;
    int acq_ = 0;
    std::queue<int> free_;

    // TODO: 高效的可重用的fd分配算法, 阻止回绕
};

} // namespace posix_quic
