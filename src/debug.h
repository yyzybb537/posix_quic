#pragma once

#include <stdio.h>
#include <string.h>
#include <string>
#include <cstdlib>

namespace posix_quic {

enum eDbgMask {
    dbg_connect     = 0x1,
    dbg_accept      = 0x1 << 1,
    dbg_write       = 0x1 << 2,
    dbg_read        = 0x1 << 3,
    dbg_close       = 0x1 << 4,
    dbg_epoll       = 0x1 << 5,
    dbg_api         = 0x1 << 6,
    dbg_ignore      = 0x1 << 7,
    dbg_event       = 0x1 << 8,
    dbg_fd          = 0x1 << 9,
    dbg_user        = 0x1 << 31,
    dbg_all         = 0xffffffff,
};

extern FILE* debug_output;
extern uint64_t debug_mask;

std::string GetCurrentTime();
int GetCurrentProcessID();
const char* BaseFile(const char* file);

class ErrnoStore {
public:
    ErrnoStore() : errno_(errno) {}
    ~ErrnoStore() {
        errno = errno_;
    }
private:
    int errno_;
};

std::string Bin2Hex(const char* data, size_t length, const std::string& split = "");
const char* PollEvent2Str(short int event);
const char* EpollEvent2Str(uint32_t event);
const char* EpollOp2Str(int op);
const char* EntryCategory2Str(int category);
const char* Perspective2Str(int perspective);
std::string Format(const char* fmt, ...) __attribute__((format(printf,1,2)));
std::string P(const char* fmt, ...) __attribute__((format(printf,1,2)));
std::string P();

enum eSourceMask {
    src_epoll       = 0x1,
    src_socket      = 0x1 << 1,
    src_stream      = 0x1 << 2,
    src_connection  = 0x1 << 3,
    src_all = 0xffffffff,
};

std::string GlobalDebugInfo(uint32_t sourceMask);

#define DebugPrint(type, fmt, ...) \
    do { \
        if ((type) == ::posix_quic::dbg_user || (::posix_quic::debug_mask & (type)) != 0) { \
            ErrnoStore es; \
            fprintf(::posix_quic::debug_output, "[%s][P%05d]%s:%d:(%s)\t " fmt "\n", \
                    ::posix_quic::GetCurrentTime().c_str(),\
                    ::posix_quic::GetCurrentProcessID(),\
                    ::posix_quic::BaseFile(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__); \
            fflush(::posix_quic::debug_output); \
        } \
    } while(0)

#define UserLog(fmt, ...) DebugPrint(::posix_quic::dbg_user, fmt, ##__VA_ARGS__)

} // namespace posix_quic
