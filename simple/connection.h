#pragma once

#include "simple_fwd.h"
#include "quic_socket.h"
#include <vector>
#include <memory>
#include <map>
#include <list>

namespace posix_quic {
namespace simple {

class IOService;

enum eWriteStream {
    AlwaysNewStream = -3,
    LastStream = -2,
    PublicStream = -1,
};

class Connection
{
    friend class IOService;

public:
    struct Context {
        uint64_t id = 0;
        bool isSocket;
        union {
            QuicSocket socket;
            QuicStream stream;
        };
        Connection * connection;
    };

    struct Stream {
        Stream(Connection * connection, QuicStream stream);
        ~Stream();

        void Close();

        bool Write(const char* data, size_t len, bool fin);

        void WriteBufferedData();

        Context ctx_;
        bool bufferedFin_ = false;
        bool writeFin_ = false;
        bool closed_ = false;
        std::list<std::string> buffers_;
    };
    typedef std::shared_ptr<Stream> StreamPtr;

public:
    // create by user
    explicit Connection(IOService* ios);

    virtual ~Connection();

    int Connect(const struct sockaddr* addr, socklen_t addrLen);

    int Accept(const struct sockaddr* addr, socklen_t addrLen);

    bool Write(const char* data, size_t len, QuicStream stream = PublicStream, bool fin = false);

    void Close();

    QuicSocket Native() const { return socket_; }

    long BufferedSliceCount(QuicStream stream);

    uint64_t ConnectionId() const { return connectionId_; }

public:
    virtual Connection* NewConnection(IOService* ios, QuicSocket socket) = 0;

    // only called by client peer.
    virtual void OnConnected() {}

    // only called by server peer.
    virtual void OnAcceptSocket(Connection* connection) {}

    virtual void OnRecv(const char* data, size_t len, QuicStream stream) {}

    virtual void OnClose(int sysError, int quicError, bool bFromRemote) {}

    virtual void OnStreamClose(int sysError, int quicError, QuicStream stream) {}

protected:
    // create by io_service
    Connection(IOService* ios, QuicSocket socket);

private:
    void OnAccept();

    void OnAcceptSocket();

    void OnAcceptStream();

    // @return: is valid.
    bool OnCanRead(QuicStream stream);

    bool OnCanWrite(QuicStream stream);

    bool OnError(QuicStream stream);

    int AddIntoService();

    void CreateIncomingStream(QuicStream stream);

    StreamPtr CreateOutgoingStream();

    StreamPtr GetStream(QuicStream stream);

private:
    QuicSocket socket_ = -1;

    Context sockCtx_;

    std::map<QuicStream, StreamPtr> streams_;

    IOService* ios_;

    bool listener_ = false;

    bool connected_ = false;

    bool closed_ = false;

    uint64_t connectionId_ = -1;
};

} // amespace simple
} // namespace posix_quic
