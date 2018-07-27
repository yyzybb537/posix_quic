/** posix风格的quic协议封装
 * 
 * quic socket与tcp socket的使用方式基本一致, 除了:
 *   1.quic socket由QuicSocket和QuicStream两部分构成, QuicSocket负责连接相关,
 * QuicStream负责数据读写相关, QuicSocket和QuicStream都要使用QuicPoll和
 * QuicEpoller监听.
 *   2.quic socket只有非阻塞模式!
 *
 *
*/
#pragma once

#include <memory>
#include <poll.h>

namespace posix_quic {

typedef int QuicSocket;
typedef int QuicStream;

// 真正的epoll fd, 可用poll、epoll监听
typedef int QuicEpoller;

QuicSocket QuicCreateSocket();

void QuicCloseSocket(QuicSocket sock);

int QuicBind(QuicSocket sock, const struct sockaddr* addr, socklen_t addrlen);

int QuicListen(QuicSocket sock, int backlog);

int QuicConnect(QuicSocket sock, const struct sockaddr* addr, socklen_t addrlen);

QuicSocket QuicSocketAccept(QuicSocket listenSock, const struct sockaddr* addr, socklen_t & addrlen);

QuicStream QuicStreamAccept(QuicSocket sock);

QuicStream QuicCreateStream(QuicSocket sock);

void QuicCloseStream(QuicStream stream);

ssize_t QuicWritev(QuicStream stream, const struct iovec* iov, int iov_count,
        bool fin);

ssize_t QuicReadv(QuicStream stream, const struct iovec* iov, int iov_count);

// poll
int QuicPoll(struct pollfd *fds, nfds_t nfds, int timeout);

// epoll
QuicEpoller QuicCreateEpoll();

void QuicCloseEpoller(QuicEpoller epfd);

int QuicEpollCtl(QuicEpoller epfd, int op, QuicStream stream, struct epoll_event *event);

int QuicEpollWait(QuicEpoller epfd, struct epoll_event *events, int maxevents, int timeout);

} // namespace posix_quic

