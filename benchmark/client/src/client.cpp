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
#include <unistd.h>
#include <atomic>
#include <thread>
#include <vector>

using namespace posix_quic;
using namespace posix_quic::simple;

std::atomic_long g_tps{0}, g_bytes{0};
const int g_bytesPerReq = 128;
const int g_connection = 100;
const int g_pipeline = 100;

long g_bufferdSlices = 0;

void show() {
    long last_tps = 0;
    long last_bytes = 0;
    for (;;) {
        sleep(1);

        long tps = g_tps - last_tps;
        long bytes = g_bytes - last_bytes;
        last_tps = g_tps;
        last_bytes = g_bytes;

        UserLog("QPS: %ld, TPS: %ld, Bytes: %ld KB    BufferdSlices: %ld",
                bytes / g_bytesPerReq, tps, bytes / 1024, g_bufferdSlices);

        g_bufferdSlices = 0;
    }
}

class ClientConnection : public Connection
{
    using Connection::Connection;

    std::vector<char> buf_;
    size_t bufLen_ = 0;
    int packetIdx_ = 0;

public:
    virtual Connection* NewConnection(IOService* ios, QuicSocket socket) {
        return new ClientConnection(ios, socket);
    }

    // only called by client peer.
    virtual void OnConnected() {
//        SetQuicSocketOpt(Native(), sockopt_stream_wmem, 1024);

        buf_.resize(g_bytesPerReq);

        for (int i = 0; i < g_pipeline; ++i) {
            std::string g_buf(g_bytesPerReq, 'a');
            *reinterpret_cast<int*>(&g_buf[0]) = i;
            Write(g_buf.c_str(), g_buf.size(), PublicStream);
        }
    }

    // only called by server peer.
    virtual void OnAcceptSocket(Connection* connection) {}

    virtual void OnRecv(const char* data, size_t len, QuicStream stream) {
        ++g_tps;
        g_bytes += len;

        size_t bytes = len;
        while (bytes) {
            size_t copyable = std::min(g_bytesPerReq - bufLen_, bytes);
            memcpy(&buf_[bufLen_], data + len - bytes, copyable);
            bufLen_ += copyable;
            bytes -= copyable;
            if (bufLen_ == g_bytesPerReq) {
                int idx = *(int*)&buf_[0];
//                UserLog("Recv idx: %d", idx);
                assert(idx == packetIdx_);
                ++packetIdx_;
                if (packetIdx_ >= g_pipeline)
                    packetIdx_ = 0;
                bufLen_ = 0;
            }
        }

        Write(data, len, stream);
        g_bufferdSlices = std::max(g_bufferdSlices, BufferedSliceCount(stream));
    }

    virtual void OnClose(int sysError, int quicError, bool bFromRemote) {
        UserLog("Close Socket fd = %d sysErr=%d, quicErr=%d, closeByPeer:%d\n",
                Native(), sysError, quicError, bFromRemote);
        delete this;
    }

    virtual void OnStreamClose(int sysError, int quicError, QuicStream stream) {
//        UserLog("Close Stream fd=%d\n", stream);
    }
};

int main() {
//    debug_mask = dbg_all & ~dbg_timer;
//    debug_mask = dbg_simple;
    debug_mask = dbg_close;

    std::thread(&show).detach();

    IOService ios;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9700);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    for (int i = 0; i < g_connection; ++i) {
        ClientConnection* c = new ClientConnection(&ios);
        c->Connect((struct sockaddr*)&addr, sizeof(addr));
    }

    ios.RunLoop();
}
