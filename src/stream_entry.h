#pragma once

#include "fwd.h"
#include <memory>
#include "entry.h"
#include "fd_factory.h"
#include "fd_manager.h"
#include "socket_entry.h"

namespace posix_quic {

class QuicStreamEntry
    : public EntryBase,
    public std::enable_shared_from_this<QuicStreamEntry>,
    private QuartcStreamInterface::Delegate
{
public:
    QuicStreamEntry(QuicSocketEntryPtr socketEntry, QuicStreamId streamId);

    EntryCategory Category() const override { return EntryCategory::Stream; }

    std::shared_ptr<int> NativeUdpFd() const override;

    ssize_t Writev(const struct iovec* iov, size_t iov_count, bool fin = false);

    ssize_t Readv(const struct iovec* iov, size_t iov_count);

    int Shutdown(int how);

    int Close() override;

public:
    template <typename ... Args>
    static QuicStreamEntryPtr NewQuicStream(Args && ... args) {
        int fd = GetFdFactory().Alloc();
        QuicStreamEntryPtr ptr(new QuicStreamEntry(std::forward<Args>(args)...));
        ptr->SetFd(fd);
        GetFdManager().Put(fd, ptr);
        return ptr;
    }
    static void DeleteQuicStream(QuicStreamEntryPtr const& ptr);

    // -----------------------------------------------------------------
    // QuartcStreamInterface::Delegate
private:
    void OnDataAvailable(QuartcStreamInterface* stream) override;

    void OnClose(QuartcStreamInterface* stream) override;

    void OnBufferChanged(QuartcStreamInterface* stream) override {}

    void OnCanWriteNewData(QuartcStreamInterface* stream) override;
    // -----------------------------------------------------------------

private:
    QuartcStreamPtr GetQuartcStream();

private:
    std::shared_ptr<int> udpSocket_;

    QuicSocketEntryWeakPtr socketEntry_;

    QuicStreamId streamId_;
};


} // namespace posix_quic
