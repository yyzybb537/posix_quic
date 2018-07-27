#pragma once
#include "stream.h"
#include "session.h"
#include "entry.h"

namespace posix_quic {

void PosixQuicStream::Initialize()
{
    SetDelegate(&delegate_);
}

void PosixQuicStream::OnDataAvailable()
{
    ((EntryBase*)this)->SetReadable(true);
}

void PosixQuicStream::OnCanWriteNewData()
{
    ((EntryBase*)this)->SetWritable(true);
}

void PosixQuicStream::OnClose() override
{
    ((EntryBase*)this)->SetError(EBADF);

    QuartcStream::OnClose();
}

} // namespace posix_quic
