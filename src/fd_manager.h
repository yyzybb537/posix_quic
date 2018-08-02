#pragma once

#include "fwd.h"
#include <unordered_map>
#include <mutex>

namespace posix_quic {

template <typename Entry>
struct FdManager
{
public:
    void Put(int fd, Entry const& entry) {
        std::unique_lock<std::mutex> lock(mtx_);
        map_[fd] = entry;
        DebugPrint(dbg_fd, "Put %s fd = %d", entry->DebugTypeInfo(), fd);
    }

    Entry Get(int fd) {
        std::unique_lock<std::mutex> lock(mtx_);
        auto iter = map_.find(fd);
        if (iter == map_.end()) return Entry();
        return iter->second;
    }

    bool Delete(int fd) {
        std::unique_lock<std::mutex> lock(mtx_);
        auto iter = map_.find(fd);
        if (iter == map_.end()) return false;
        Entry & entry = iter->second;
        DebugPrint(dbg_fd, "Del %s fd = %d", entry->DebugTypeInfo(), fd);
        map_.erase(iter);
        return true;
    }

private:
    std::mutex mtx_;
    std::unordered_map<int, Entry> map_;
};

} // namespace posix_quic
