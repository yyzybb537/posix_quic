#include "io_service.h"
#include "connection.h"
#include "debug.h"

namespace posix_quic {
namespace simple {

IOService::IOService()
{
    ep_ = QuicCreateEpoll();
}

void IOService::Run(int timeout)
{
    struct epoll_event evs[1024];
    int n = QuicEpollWait(ep_, evs, 1024, timeout);

    DebugPrint(dbg_simple, "QuicEpollWait returns %d", n);

    for (int i = 0; i < n; ++i) {
        struct epoll_event & ev = evs[i];
        Connection::Context * ctx = (Connection::Context *)ev.data.ptr;

        if (ev.events & EPOLLIN) {
            ctx->connection->OnCanRead(ctx->isSocket ? -1 : ctx->stream);
        }

        if (ev.events & EPOLLOUT) {
            ctx->connection->OnCanWrite(ctx->isSocket ? -1 : ctx->stream);
        }

        if (ev.events & EPOLLERR) {
            ctx->connection->OnError(ctx->isSocket ? -1 : ctx->stream);
        }
    }
}

void IOService::RunLoop()
{
    for (;;) {
        Run(6000);
    }
}

} // amespace simple
} // namespace posix_quic
