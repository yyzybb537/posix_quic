#pragma once

#include "net/quic/quartc/quartc_stream.h"

namespace posix_quic {

// 自定义Stream
// OnDataAvailible时不再立即读取数据, 仅设置readable标志
class PosixQuicStream : public net::QuartcStream
{
public:
    class StreamDelegate : private net::QuartcStreamInterface::Delegate {
    public:
        void OnReceived(QuartcStreamInterface* stream,
                const char* data, size_t size) override {}

        void OnClose(QuartcStreamInterface* stream) override {}

        void OnBufferChanged(QuartcStreamInterface* stream) override {}

        void OnCanWriteNewData() override;
    };

public:
    using QuartcStream::QuartcStream;

    void Initialize();

    // readable
    void OnDataAvailable() override;

    // writable
    void OnCanWriteNewData() override;

    void OnClose() override;

private:
    FakeDelegate delegate_;
};

} // namespace posix_quic
