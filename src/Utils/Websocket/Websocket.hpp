#pragma once

#include "Shared/Network/NetProxy.hpp"


class CWebsocket final : public CListNode<CWebsocket>
{
public:
    struct context;
    struct netevent;
    struct container;
    
    enum closecode : uint16
    {
        closecode_normal    = 1000,
        closecode_away      = 1001,
        closecode_protoerr  = 1002,
        closecode_data      = 1003,
        closecode_rsv0      = 1004,
        closecode_status    = 1005,
        closecode_dc        = 1006,
        closecode_frame     = 1007,
        closecode_policy    = 1008,
        closecode_bigmsg    = 1009,
        closecode_ext       = 1010,
        closecode_bad       = 1011,

        closecodenum,
    };

    using ConnectCallback = std::function<void(CWebsocket& ws)>;
    using DisconnectCallback = std::function<void(CWebsocket& ws, closecode code)>;
    using ReadCallback = std::function<void(CWebsocket& ws, const char* data, uint32 datalen, bool binflag)>;
    using PongCallback = std::function<void(CWebsocket& ws, const char* data, uint32 datalen)>;
    using ErrorCallback = std::function<void(CWebsocket&, int32 errcode)>;

    static void initialize(void);
    static void terminate(void);

    //
    //  Walking through all opened websocket connection and forcefully abort it without close frame
    //  If bool flag specified, function waits till all connections shutdown before return
    //
    static void abort_all(bool await_clear = false);

    //
    //  Converts errcode returned by on_error to string
    //
    static const char* errcode_to_str(int32 code);

    //
    //  Converts closecode returned by on_disconnect to string
    //
    static const char* closecode_to_str(closecode code);
    
public:
    CWebsocket(void);
    ~CWebsocket(void);

    //
    //  Forcefully aborts connection without sending close frame data just closing socket
    //  note 1: awaits all threads leave user callbacks
    //  note 2: object used in user proc should be valid while destructing pending request
    //  note 3: no any event will post after shutdown
    //
    void shutdown(void);

    //
    //  Abort connection without sending close frame data
    //  If connection is active - disconnect event will send to callback
    //
    void abort(void);

    //
    //  Connects to ws server with specified URL
    //  Current connection must be not opened before and request should not be empty (use set_request)
    //  For starting connect requires at least specified callbacks: on_connect, on_error
    //  On success: on_connect callback will be called
    //  On failure: on_error callback will be called with appropriate errcode (use errcode_to_str to get details)
    //
    bool connect(const std::string& Url, uint32 ConnectTimeout = 0);

    //
    //  Send binary frame with specified data
    //  On success: on_read callback will be called with received data and flag that specified data type
    //  On failrue: on_disconnect - any error during security/transport layers abort() will called internally
    //
    void send_bin(const char* Data, uint32 DataSize);

    //
    //  Send text frame with specified data
    //  For results description see send_bin
    //
    void send_txt(const char* Data, uint32 DataSize);

    //
    //  Send ping frame with specified data
    //  Due ws protocol frame limits data size should not be greater that 125, otherwise it will be truncated to that boundary
    //  For results description see send_bin
    //
    void ping(const char* Data = nullptr, uint32 DataSize = 0);

    //
    //  Send close frame with specified closecode and reason
    //  Due ws protocol frame limits data size should not be greater that 123, otherwise it will be truncated to that boundary
    //  For results description see send_bin
    //
    void close(closecode code = closecode_normal, const char* Data = nullptr, uint32 DataSize = 0);

    //
    //  Sets callback that notifies about success connection
    //
    void on_connect(ConnectCallback cb);

    //
    //  Sets callback that notifies about disconnect
    //
    void on_disconnect(DisconnectCallback cb);

    //
    //  Sets callback that notifies about incoming data
    //
    void on_read(ReadCallback cb);

    //
    //  Sets callback that notifies about software errors
    //
    void on_error(ErrorCallback cb);

    //
    //  Sets callback that notifies about incoming frame with PONG opcode 
    //
    void on_pong(PongCallback cb);

    //
    //  Sets timeouts for reading
    //
    void set_timeout(uint32 ms);

    //
    //  Sets request that will be send to connect to websocket server
    //  Internal impl do not provide own construction for request thus should be set for success connection
    //
    void set_request(const std::string& req);

    //
    //  Sets websocket flag that data should be masked
    //
    void set_masking(bool flag);

    //
    //  Sets websocket flag that data should be compressed
    //
    void set_compress(bool flag);

    //
    //  Sets proxy for socket layer
    //
    void set_proxy(NetProxy_t ProxyType, uint64 NetAddr, void* Parameter = nullptr, int32 ParameterLen = 0);

    //
    //  Infinitely awaits for socket complete session
    //  Under complete session means that socket successfully connect and disconnect from server or error occur
    //
    void wait(void);

    //
    //  Waits specified milliseconds for request completion or error
    //  Under complete session means that socket successfully connect and disconnect from server or error occur
    //  On sucess: returns true if socket complete session
    //  On failure: returns false if timeout occur
    //
    bool wait(uint32 ms);

private:
    bool send_common(const char* Data, uint32 DataSize, int32 Opcode);
    context& ctx(void);

private:
    context* m_pContext;
};