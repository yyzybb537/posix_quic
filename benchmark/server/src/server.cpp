#include "simple_quic.h"
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
#include <atomic>
#include <thread>
#include <unordered_map>
//#include "../../profiler.h"

using namespace posix_quic;
using namespace posix_quic::simple;

std::atomic_long g_tps{0}, g_bytes{0};

std::unordered_map<class ServerConnection*, int> g_delMap;

class ServerConnection : public Connection
{
public:
    static std::atomic_long nConnections;

    ServerConnection(IOService* ios) : Connection(ios) {
    }

    ServerConnection(IOService* ios, QuicSocket socket) : Connection(ios, socket) {
        ++nConnections;
    }

    ~ServerConnection() {
        --nConnections;
    }

public:
    virtual Connection* NewConnection(IOService* ios, QuicSocket socket) {
        return new ServerConnection(ios, socket);
    }

    // only called by client peer.
    virtual void OnConnected() {}

    // only called by server peer.
    virtual void OnAcceptSocket(Connection* connection) {
        // nothing to do
        UserLog("OnAcceptSocket fd=%d", connection->Native());
    }

    virtual void OnRecv(const char* data, size_t len, QuicStream stream) {
//        UserLog("recv: %.*s", (int)len, data);

        ++g_tps;
        g_bytes += len;
        Write(data, len, stream);
    }

    virtual void OnClose(int sysError, int quicError, bool bFromRemote) {
        UserLog("Close Socket fd=%d sysErr=%d, quicErr=%d, closeByPeer:%d\n",
                Native(), sysError, quicError, bFromRemote);
        if (g_delMap.count(this)) {
            UserLog("double free!");
        } else {
            g_delMap[this] = 1;
        }
        delete this;
    }

    virtual void OnStreamClose(int sysError, int quicError, QuicStream stream) {
//        UserLog("Close Stream fd=%d\n", stream);
    }
};
std::atomic_long ServerConnection::nConnections{0};

void show() {
    long last_tps = 0;
    long last_bytes = 0;
    for (;;) {
        sleep(1);

        long tps = g_tps - last_tps;
        long bytes = g_bytes - last_bytes;
        last_tps = g_tps;
        last_bytes = g_bytes;

        UserLog("TPS: %ld, Bytes: %ld KB, Conn: %ld",
                tps, bytes / 1024, (long)ServerConnection::nConnections);
    }
}

int main() {
//    debug_mask = dbg_all & ~dbg_timer;
    debug_mask = dbg_close | dbg_ack_timeout | dbg_conn_visitor;
//    debug_mask = dbg_simple;

//    GProfiler::Initialize();

    std::thread(&show).detach();
    
    IOService ios;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9700);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    ServerConnection s(&ios);
    int res = s.Accept((struct sockaddr*)&addr, sizeof(addr));
    if (res < 0) {
        perror("Accept error");
        exit(1);
    }

//    ios.RunLoop();
    for (;;) {
        size_t n = ios.Run(6000);
        UserLog("EpollWait %d", (int)n);
    }
}
