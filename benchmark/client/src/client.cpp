#include "quic_socket.h"
#include "debug.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <string>
#include <sys/epoll.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <atomic>
#include <thread>

using namespace posix_quic;

#define CHECK_RES(res, api) \
    do {\
        if (res < 0) {\
            perror(api " error");\
            return 1;\
        }\
    } while (0)

#define CHECK_RES_ALLOW_EAGAIN(res, api) \
    do {\
        if (res < 0 && errno != EAGAIN) {\
            perror(api " error");\
            return 1;\
        }\
    } while (0)

std::atomic_long g_qps{0}, g_bytes{0};
const std::string g_buf(1400, 'a');

void show() {
    long last_qps = 0;
    long last_bytes = 0;
    for (;;) {
        sleep(1);

        long qps = g_qps - last_qps;
        long bytes = g_bytes - last_bytes;
        last_qps = g_qps;
        last_bytes = g_bytes;

        UserLog("QPS: %ld, Bytes: %ld KB", qps, bytes / 1024);
    }
}

int OnRead(QuicStream fd) {
    int res;
    char buf[10240];
    for (;;) {
        res = QuicRead(fd, buf, sizeof(buf));
        if (res < 0) {
            if (errno == EAGAIN)
                return 0;

            QuicCloseStream(fd);
            return 1;
        } else if (res == 0) {
            QuicStreamShutdown(fd, SHUT_WR);
            return 1;
        }

        ++g_qps;
        g_bytes += res;

//        UserLog("recv(len=%d): %.*s\n", res, res, buf);

        res = QuicWrite(fd, buf, res, false);
//        CHECK_RES(res, "write");
    }
}

int doLoop(QuicEpoller ep) {
    struct epoll_event evs[1024];
    int n = QuicEpollWait(ep, evs, sizeof(evs)/sizeof(struct epoll_event), 6000);
    CHECK_RES(n, "epoll_wait");

    int res;
    for (int i = 0; i < n; i++) {
        struct epoll_event & ev = evs[i];
        int fd = ev.data.fd;
        EntryCategory category = GetCategory(fd);

//        UserLog("QuicEpoller trigger: fd = %d, category = %s, events = %s", fd, EntryCategory2Str((int)category), EpollEvent2Str(ev.events));

        if (ev.events & EPOLLOUT) {
            // connected
            UserLog("Connected.\n");

            if (category == EntryCategory::Socket) {
                QuicStream stream = QuicCreateStream(fd);
                assert(stream > 0);

                struct epoll_event ev;
                ev.data.fd = stream;
                ev.events = EPOLLIN;
                res = QuicEpollCtl(ep, EPOLL_CTL_ADD, stream, &ev);
                CHECK_RES(res, "epoll_ctl");

                res = QuicWrite(stream, g_buf.c_str(), g_buf.size(), false);
                CHECK_RES(res, "write");
            }
        }

        if (ev.events & EPOLLIN) {
//            UserLog("QuicDebugInfo:\n%s\n", GlobalDebugInfo(src_all).c_str());

            if (category == EntryCategory::Socket) {
                // client needn't accept
            } else if (category == EntryCategory::Stream) {
                // stream, recv data.
                OnRead(fd);
            }
        }

        if (ev.events & EPOLLERR) {
            if (category == EntryCategory::Socket) {
                UserLog("Close Socket fd=%d\n", fd);
                QuicCloseSocket(fd);
            } else if (category == EntryCategory::Stream) {
                UserLog("Close Stream fd=%d\n", fd);
                QuicCloseStream(fd);
            } 
        }
    }

    return 0;
}

int main() {
//    debug_mask = dbg_all & ~dbg_timer;
    std::thread(&show).detach();

    QuicEpoller ep = QuicCreateEpoll();
    assert(ep >= 0);

    QuicSocket socket = QuicCreateSocket();
    assert(socket > 0);

    int res;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9700);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    res = QuicConnect(socket, (struct sockaddr*)&addr, sizeof(addr));
    assert(errno == EINPROGRESS);
    assert(res == -1);

    struct epoll_event ev;
    ev.data.fd = socket;
    ev.events = EPOLLIN | EPOLLOUT;
    res = QuicEpollCtl(ep, EPOLL_CTL_ADD, socket, &ev);
    CHECK_RES(res, "epoll_ctl");

    for (;;) {
        res = doLoop(ep);
        if (res != 0)
            return res;
    }
}
