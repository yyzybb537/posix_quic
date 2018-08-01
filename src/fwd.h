#pragma once

#include <memory>
#include "net/quic/quartc/quartc_session.h"
#include "net/quic/quartc/quartc_session_interface.h"
#include "net/quic/quartc/quartc_factory.h"
#include "net/quic/quartc/quartc_task_runner_interface.h"

namespace posix_quic {

#define INVALID_QUIC_CONNECTION_ID QuicConnectionId(-1)

using net::QuicConnectionId;
using net::QuicStreamId;
using QuicSocketAddress = net::QuicSocketAddress;
using QuartcStream = net::QuartcStream;
using QuartcStreamInterface = net::QuartcStreamInterface;
using QuartcSession = net::QuartcSession;
using QuartcSessionInterface = net::QuartcSessionInterface;
using QuicErrorCode = net::QuicErrorCode;
using QuicReceivedPacket = net::QuicReceivedPacket;
using QuartcFactory = net::QuartcFactory;
using QuicTime = net::QuicTime;
using QuartcTaskRunnerInterface = net::QuartcTaskRunnerInterface;
using QuicConsumedData = net::QuicConsumedData;

typedef int UdpSocket;
typedef int QuicSocket;
typedef int QuicStream;

typedef std::shared_ptr<QuartcStream> QuartcStreamPtr;

class QuicSocketEntry;
typedef std::weak_ptr<QuicSocketEntry> QuicSocketEntryWeakPtr;
typedef std::shared_ptr<QuicSocketEntry> QuicSocketEntryPtr;

class QuicStreamEntry;
typedef std::weak_ptr<QuicStreamEntry> QuicStreamEntryWeakPtr;
typedef std::shared_ptr<QuicStreamEntry> QuicStreamEntryPtr;

class QuicEpollerEntry;
typedef std::weak_ptr<QuicEpollerEntry> QuicEpollerEntryWeakPtr;
typedef std::shared_ptr<QuicEpollerEntry> QuicEpollerEntryPtr;

} // namespace posix_quic
