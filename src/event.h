#pragma once

#include <memory>

namespace posix_quic {

struct Event
{
    bool readable = false;
    bool writable = false;
    int error = 0;
    QuicErrorCode quicErrorCode = QUIC_NO_ERROR;
    bool closeByPeer = false;

    virtual ~Event() {}

    void SetReadable(bool b) { readable = b; }
    void SetWritable(bool b) { writable = b; }
    void SetError(int err, int quicErr = 0) {
        if (error == 0) {
            error = err;
            quicErrorCode = quicErr;
        }
    }
    void SetCloseByPeer(bool b) { closeByPeer = b; }

    bool Readable() { return readable; }
    bool Writable() { return writable; }
    int Error() { return error; }
    bool IsCloseByPeer() { return closeByPeer; }
    QuicErrorCode GetQuicErrorCode() { return quicErrorCode; }
};

} // namespace posix_quic
