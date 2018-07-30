#include "stream_entry.h"

namespace posix_quic {

QuicStreamEntry::QuicStreamEntry()
{
    auto stream = GetQuartcStream();
    if (stream)
        stream->SetDelegate(this);
}

ssize_t QuicStreamEntry::Writev(const struct iovec* iov, size_t iov_count, bool fin)
{
    if (error) {
        errno = error;
        return -1;
    }

    auto stream = GetQuartcStream();
    if (!stream) {
        errno = EBADF;
        return -1;
    }

    QuicConsumedData resData = stream->WritevData(iov, iov_count, fin);
    if (resData.bytes_consumed == 0) {
        errno = EAGAIN;
        return -1;
    }

    return resData.bytes_consumed;
}

ssize_t QuicStreamEntry::Readv(const struct iovec* iov, size_t iov_count)
{
    if (error) {
        errno = error;
        return -1;
    }

    auto stream = GetQuartcStream();
    if (!stream) {
        errno = EBADF;
        return -1;
    }

    int res = stream->Readv(iov, iov_count);
    if (res == 0) {
        errno = EAGAIN;
        return -1;
    }

    return res;
}

int QuicStreamEntry::Shutdown(int how)
{
    auto stream = GetQuartcStream();
    if (!stream) {
        errno = EBADF;
        return -1;
    }

    if (how == SHUT_RD) {
        stream->FinishReading();
    } else if (how == SHUT_WR) {
        stream->FinishWriting();
    } else if (how == SHUT_RDWR) {
        stream->FinishReading();
        stream->FinishWriting();
    }
    return 0;
}

int QuicStreamEntry::Close()
{
    auto stream = GetQuartcStream();
    if (!stream) {
        errno = EBADF;
        return -1;
    }

    stream->Close();
    return 0;
}
FdFactory & QuicStreamEntry::GetFdFactory()
{
    static FdFactory obj;
    return obj;
}
FdManager<QuicStreamEntryPtr> & QuicStreamEntry::GetFdManager()
{
    static FdManager<QuicStreamEntryPtr> obj;
    return obj;
}
void QuicStreamEntry::DeleteQuicStream(QuicStreamEntryPtr const& ptr)
{
    int fd = ptr->Fd();
    if (fd >= 0) {
        GetFdManager().Delete(fd);
        GetFdFactory().Free(fd);
        ptr->SetFd(-1);
    }
}

void QuicStreamEntry::OnDataAvailable() override
{
    SetReadable(true);
}

void QuicStreamEntry::OnClose(QuartcStreamInterface* stream) override
{
    SetError(EBADF);
}

void QuicStreamEntry::OnCanWriteNewData() override
{
    SetWritable(true);
}

QuartcStreamPtr QuicStreamEntry::GetQuartcStream()
{
    // 设置deleter：unlock session::mutex.
    auto socket = socketEntry_.lock();
    if (!socket) return QuartcStreamPtr();
    return socket->GetQuartcStream(streamId_);
}

};
