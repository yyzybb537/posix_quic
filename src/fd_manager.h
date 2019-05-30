#pragma once

#include "fwd.h"
#include <unordered_map>
#include <mutex>
#include <set>

namespace posix_quic {

template <typename Entry>
struct FdManager
{
public:
    ~FdManager() {
        DebugPrint(dbg_fd, "FdManager destroy. %p", this);
    }

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

    template <typename F>
    void Foreach(F const& f) {
        std::unique_lock<std::mutex> lock(mtx_);
        std::set<int> fds;
        for (auto & kv : map_) {
            fds.insert(kv.first);
        }
        for (auto & fd : fds) {
            auto iter = map_.find(fd);
            if (map_.end() != iter)
                f(fd, iter->second);
        }
    }

private:
    std::mutex mtx_;
    std::unordered_map<int, Entry> map_;
};

} // namespace posix_quic
