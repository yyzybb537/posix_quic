#include "quic_socket.h"
#include <string.h>
#include <stdio.h>
#include <string>
#include <sys/epoll.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>

using namespace posix_quic;

#define CHECK_RES(res, api) \
    do {\
        if (res < 0) {\
            perror(api " error");\
            return 1;\
        }\
    } while (0)

bool gIsTestReset = false;
bool gIsTestSocketReset = false;
bool gIsTestSocketResetWithStream = false;

QuicSocket conn = -1;

void onStreamClose() {
    if (gIsTestSocketReset) {
        QuicCloseSocket(conn);
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

        UserLog("QuicEpoller trigger: fd = %d, category = %s, events = %s",
                fd, EntryCategory2Str((int)category), EpollEvent2Str(ev.events));

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

                std::string s = "Hello quic!";
                res = QuicWrite(stream, s.c_str(), s.size(), false);
                CHECK_RES(res, "write");
            }
        }

        if (ev.events & EPOLLIN) {
            UserLog("QuicDebugInfo:\n%s\n", GlobalDebugInfo(src_all).c_str());

            if (category == EntryCategory::Socket) {
                // client needn't accept
            } else if (category == EntryCategory::Stream) {
                // stream, recv data.
                char buf[10240];
                res = QuicRead(fd, buf, sizeof(buf));
                CHECK_RES(res, "read");

                UserLog("recv(len=%d): %.*s\n", res, res, buf);

                if (gIsTestSocketResetWithStream)
                    QuicCloseSocket(conn);
                else if (std::string(buf, res) != "Bye") {
                    std::string s = "Bye";
                    if (gIsTestReset) {
                        res = QuicWrite(fd, s.c_str(), s.size(), false);
                        CHECK_RES(res, "write");
                        QuicCloseStream(fd);
                        onStreamClose();
                    } else {
                        res = QuicWrite(fd, s.c_str(), s.size(), true);
                        CHECK_RES(res, "write");
                    }
                }
            }
        }

        if (ev.events & EPOLLERR) {
            if (category == EntryCategory::Socket) {
                UserLog("Close Socket fd=%d\n", fd);
                QuicCloseSocket(fd);
            } else if (category == EntryCategory::Stream) {
                UserLog("Close Stream fd=%d\n", fd);
                QuicCloseStream(fd);
                onStreamClose();
            } 
        }
    }

    return 0;
}

int main() {
    debug_mask = dbg_all & ~dbg_timer;

    QuicEpoller ep = QuicCreateEpoll();
    assert(ep >= 0);

    QuicSocket socket = QuicCreateSocket();
    assert(socket > 0);
    conn = socket;

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
