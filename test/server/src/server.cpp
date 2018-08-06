#include "quic_socket.h"
#include "debug.h"
#include <string.h>
#include <stdio.h>
#include <string>
#include <sys/epoll.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace posix_quic;

#define CHECK_RES(res, api) \
    do {\
        if (res < 0) {\
            perror(api " error");\
            return 1;\
        }\
    } while (0)

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

        UserLog("recv(len=%d): %.*s\n", res, res, buf);

        res = QuicWrite(fd, buf, res, false);
        CHECK_RES(res, "write");
    }
}

int doLoop(QuicEpoller ep, QuicSocket listenSock) {
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
            // 此次测试中, 协议栈5MB的缓冲区足够使用, 不再创建额外的用户层缓冲区.
            UserLog("Ignore EPOLLOUT\n");
        }

        if (ev.events & EPOLLIN) {
            if (category == EntryCategory::Socket) {
                if (fd == listenSock) {
                    // accept socket
//                    UserLog("QuicDebugInfo:\n%s\n", GlobalDebugInfo(src_all).c_str());
                    for (;;) {
                        QuicSocket newSocket = QuicSocketAccept(fd);
                        if (newSocket > 0) {
                            struct epoll_event ev;
                            ev.data.fd = newSocket;
                            ev.events = EPOLLIN;
                            res = QuicEpollCtl(ep, EPOLL_CTL_ADD, newSocket, &ev);
                            CHECK_RES(res, "epoll_ctl");

                            UserLog("Accept Socket fd=%d, newSocket=%d\n", fd, newSocket);
                        } else {
                            UserLog("No Accept Socket. fd=%d\n", fd);
                            break;
                        }
                    }
                } else {
                    // accept stream
                    for (;;) {
                        QuicStream newStream = QuicStreamAccept(fd);
                        if (newStream > 0) {
                            struct epoll_event ev;
                            ev.data.fd = newStream;
                            ev.events = EPOLLIN;
                            res = QuicEpollCtl(ep, EPOLL_CTL_ADD, newStream, &ev);
                            CHECK_RES(res, "epoll_ctl");
                            UserLog("Accept Stream fd=%d, newSocket=%d\n", fd, newStream);

                            OnRead(newStream);
                        } else {
                            UserLog("No Accept Stream. fd=%d\n", fd);
                            break;
                        }
                    }
                }

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
    debug_mask = dbg_all & ~dbg_timer;
    
    QuicEpoller ep = QuicCreateEpoll();
    assert(ep >= 0);

    QuicSocket socket = QuicCreateSocket();
    assert(socket > 0);

    int res;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9700);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    res = QuicBind(socket, (struct sockaddr*)&addr, sizeof(addr));
    CHECK_RES(res, "bind");

    struct epoll_event ev;
    ev.data.fd = socket;
    ev.events = EPOLLIN | EPOLLOUT;
    res = QuicEpollCtl(ep, EPOLL_CTL_ADD, socket, &ev);
    CHECK_RES(res, "epoll_ctl");

    for (;;) {
        res = doLoop(ep, socket);
        if (res != 0)
            return res;
    }
}
