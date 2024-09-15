#pragma once

#include "Shared/Network/NetProxy.hpp"


class CWebsocket final
{
public:
    /**
     *  NOTE: Websocket client only impl
     */
    
    struct context;
    
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

    using ConnectCallback   = std::function<void(CWebsocket& ws)>;
    using DisconnectCallback= std::function<void(CWebsocket& ws, closecode code)>;
    using ReadCallback      = std::function<void(CWebsocket& ws, const void* data, std::size_t size, bool binary)>;
    using PongCallback      = std::function<void(CWebsocket& ws, const void* data, std::size_t size)>;
    using ErrorCallback     = std::function<void(CWebsocket& ws, uint32 errcode)>;

public:
    static const char* errcode_to_str(uint32 code);
    static const char* closecode_to_str(closecode code);
    
    CWebsocket();
    CWebsocket(CWebsocket&& r);
    CWebsocket(const CWebsocket& r);
    CWebsocket& operator=(const CWebsocket& r);
    ~CWebsocket();
    
    void connect(const std::string& url, std::chrono::milliseconds timeout = std::chrono::milliseconds(0));
    void connect(const std::string& url, const std::string& request, std::chrono::milliseconds timeout = std::chrono::milliseconds(0));
    void connect_ex(const std::string& url, const std::string& wsExtensions, std::chrono::milliseconds timeout = std::chrono::milliseconds(0));
    void send_bin(const void* data, std::size_t size);
    void send_txt(const void* data, std::size_t size);
    void ping(const void* data = nullptr, std::size_t size = 0u);
    void close(closecode code = closecode_normal, const void* data = nullptr, std::size_t size = 0u);
    void abort();
    
    void on_connect(ConnectCallback cb);
    void on_disconnect(DisconnectCallback cb);
    void on_read(ReadCallback cb);
    void on_error(ErrorCallback cb);
    void on_pong(PongCallback cb);

    void set_timeout(std::chrono::milliseconds ms);
    void set_masking(bool state);
    void set_compress(bool state);
    void set_proxy(NETPROXY type, uint64 netaddr, const NETPROXYPARAM* param = nullptr);
    void clear_proxy();
    void resolve_redirect(bool state, int32 depth = 7);

    void wait();
    bool wait(std::chrono::milliseconds ms);

private:
    context& ctx();

private:
    context* m_pContext;
};