#pragma once

#include <memory>

namespace posix_quic {

struct Event
{
    bool readable = false;
    bool writable = false;
    int error = 0;

    virtual ~Event() {}

    void SetReadable(bool b) { readable = b; }
    void SetWritable(bool b) { writable = b; }
    void SetError(int err) { if (error == 0) error = err; }

    bool Readable() { return readable; }
    bool Writable() { return writable; }
    int Error() { return error; }

    void ClearError() { error = 0; }
};

} // namespace posix_quic
