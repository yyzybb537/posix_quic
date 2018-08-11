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
        uint64_t ctxId = ev.data.u64;
        Connection::Context* evCtx = GetContext(ctxId);
        if (!evCtx) continue;

        Connection::Context back = *evCtx;
        Connection::Context* ctx = &back;

        if (ev.events & EPOLLIN) {
            if (!ctx->connection->OnCanRead(ctx->isSocket ? -1 : ctx->stream))
                continue;
        }

        if (ev.events & EPOLLOUT) {
            if (!ctx->connection->OnCanWrite(ctx->isSocket ? -1 : ctx->stream))
                continue;
        }

        if (ev.events & EPOLLERR) {
            if (!ctx->connection->OnError(ctx->isSocket ? -1 : ctx->stream))
                continue;
        }
    }
}

void IOService::RunLoop()
{
    for (;;) {
        Run(6000);
    }
}

uint64_t IOService::AddContext(Connection::Context* ctx)
{
    std::unique_lock<std::mutex> lock(contextTableMtx_);
    uint64_t id = ++ctxIdInc_;
    contextTable_[id] = ctx;
    return id;
}

void IOService::DelContext(uint64_t ctxId)
{
    std::unique_lock<std::mutex> lock(contextTableMtx_);
    contextTable_.erase(ctxId);
}

Connection::Context* IOService::GetContext(uint64_t ctxId)
{
    std::unique_lock<std::mutex> lock(contextTableMtx_);
    auto itr = contextTable_.find(ctxId);
    if (itr == contextTable_.end()) return nullptr;
    return itr->second;
}

} // amespace simple
} // namespace posix_quic
