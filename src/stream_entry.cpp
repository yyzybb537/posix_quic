#include "stream_entry.h"

namespace posix_quic {

QuicStreamEntry::QuicStreamEntry(QuicSocketEntryPtr socketEntry, QuartcStreamInterface* stream)
{
    udpSocket_ = socketEntry->NativeUdpFd();
    socketEntry_ = socketEntry;
    streamId_ = stream->stream_id();
    stream->SetDelegate(this);
}

std::shared_ptr<int> QuicStreamEntry::NativeUdpFd() const
{
    return udpSocket_;
}

ssize_t QuicStreamEntry::Writev(const struct iovec* iov, size_t iov_count, bool fin)
{
    if (Error()) {
        DebugPrint(dbg_write, "stream = %d, Has error = %d", Fd(), Error());
        errno = Error();
        return -1;
    }

    auto stream = GetQuartcStream();
    if (!stream) {
        DebugPrint(dbg_write, "stream = %d, GetQuartcStream returns nullptr", Fd());
        errno = EBADF;
        return -1;
    }

    QuicConsumedData resData = stream->WritevData(iov, iov_count, fin);
    if (resData.bytes_consumed == 0) {
        errno = EAGAIN;
        return -1;
    }

    errno = 0;
    return resData.bytes_consumed;
}

ssize_t QuicStreamEntry::Readv(const struct iovec* iov, size_t iov_count)
{
    if (Error()) {
        errno = Error();
        return -1;
    }

    if (finRead_) {
        errno = 0;
        return 0;
    }

    auto stream = GetQuartcStream();
    if (!stream) {
        errno = EBADF;
        return -1;
    }

    int res = stream->Readv(iov, iov_count);
    if (res == 0) {
        if (Error()) {
            errno = Error();
            return -1;
        }

        if (finRead_) {
            errno = 0;
            return 0;
        }

        errno = EAGAIN;
        return -1;
    }

    errno = 0;
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
    if (stream) {
        stream->Close();  // will call this->OnClose to SetError
    }

    ClearWaitingsByClose();
    return 0;
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

void QuicStreamEntry::OnDataAvailable(QuartcStreamInterface* stream)
{
    SetReadable(true);
}

void QuicStreamEntry::OnClose(QuartcStreamInterface* stream)
{
    SetError(EBADF);
}

void QuicStreamEntry::OnCanWriteNewData(QuartcStreamInterface* stream)
{
    SetWritable(true);
}

void QuicStreamEntry::OnFinRead(QuartcStreamInterface* stream)
{
    if (finRead_)
        return ;

    DebugPrint(dbg_read, "stream fd = %d", Fd());
    finRead_ = true;
}

QuicByteCount QuicStreamEntry::GetBufferedDataThreshold(QuicByteCount defaultThreshold) const
{
    auto socket = socketEntry_.lock();
    if (!socket) return defaultThreshold;

    QuicByteCount threshold = socket->GetOpt(sockopt_stream_wmem);
    if (!threshold) return defaultThreshold;

    return threshold;
}

QuartcStreamPtr QuicStreamEntry::GetQuartcStream()
{
    // 设置deleter：unlock session::mutex.
    auto socket = socketEntry_.lock();
    if (!socket) return QuartcStreamPtr();
    return socket->GetQuartcStream(streamId_);
}

std::string QuicStreamEntry::GetDebugInfo(int indent)
{
    std::string idt(indent, ' ');
    std::string idt2 = idt + ' ';
    std::string idt3 = idt2 + ' ';

    std::string info;
    info += idt + P("=========== Stream: %d ===========", Fd());
    auto socket = socketEntry_.lock();
    info += idt2 + P("From socket: %d", socket ? socket->Fd() : -1);

    info += Event::GetDebugInfo(indent + 1);
    return info;
}

};
