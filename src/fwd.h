#pragma once

#include "fwd_ext.h"
#include <memory>
#include "net/quic/quartc/quartc_session.h"
#include "net/quic/quartc/quartc_session_interface.h"
#include "net/quic/quartc/quartc_factory.h"
#include "net/quic/quartc/quartc_task_runner_interface.h"
#include "debug.h"

namespace posix_quic {

using net::QuicConnectionId;
using net::QuicStreamId;
using QuicSocketAddress = net::QuicSocketAddress;
using QuartcStream = net::QuartcStream;
using QuartcStreamInterface = net::QuartcStreamInterface;
using QuartcSession = net::QuartcSession;
using QuicConnection = net::QuicConnection;
using QuicAlarm = net::QuicAlarm;
using QuartcSessionInterface = net::QuartcSessionInterface;
using QuicErrorCode = net::QuicErrorCode;
using QuicReceivedPacket = net::QuicReceivedPacket;
using QuartcFactory = net::QuartcFactory;
using QuicTime = net::QuicTime;
using QuartcTaskRunnerInterface = net::QuartcTaskRunnerInterface;
using QuicConsumedData = net::QuicConsumedData;
using QuicFramer = net::QuicFramer;
using QuicFramerVisitorInterface = net::QuicFramerVisitorInterface;
using QuicPacketHeader = net::QuicPacketHeader;
using QuicEncryptedPacket = net::QuicEncryptedPacket;
using SerializedPacket = net::SerializedPacket;
using TransmissionType = net::TransmissionType;
using QuicAckFrame = net::QuicAckFrame;
using QuicPacketNumber = net::QuicPacketNumber;
using QuicStreamFrame = net::QuicStreamFrame;
using QuicStopWaitingFrame = net::QuicStopWaitingFrame;
using QuicPaddingFrame = net::QuicPaddingFrame;
using QuicPingFrame = net::QuicPingFrame;
using QuicGoAwayFrame = net::QuicGoAwayFrame;
using QuicRstStreamFrame = net::QuicRstStreamFrame;
using QuicConnectionCloseFrame = net::QuicConnectionCloseFrame;
using QuicWindowUpdateFrame = net::QuicWindowUpdateFrame;
using QuicBlockedFrame = net::QuicBlockedFrame;
using QuicPublicResetPacket = net::QuicPublicResetPacket;
using QuicVersionNegotiationPacket = net::QuicVersionNegotiationPacket;
using QuicString = net::QuicString;
using ConnectionCloseSource = net::ConnectionCloseSource;
using ParsedQuicVersion = net::ParsedQuicVersion;
using QuicConfig = net::QuicConfig;
using CachedNetworkParameters = net::CachedNetworkParameters;

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
