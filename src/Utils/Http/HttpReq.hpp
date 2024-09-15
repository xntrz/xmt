#pragma once

#include "HttpResponse.hpp"

#include "Shared/Network/NetProxy.hpp"
#include "Shared/Network/NetTypes.hpp"


class CHttpReq final
{
public:
    struct context;

    using CompleteCallback = std::function<bool(CHttpReq& req, CHttpResponse& resp)>;
    using ErrorCallback = std::function<void(CHttpReq& req, uint32 errcode)>;

public:
    static const char* errcode_to_str(uint32 errcode);
    static bool errcode_is_user(uint32 errcode);
    
    CHttpReq();
    CHttpReq(CHttpReq&& r);
    CHttpReq(const CHttpReq& r);
    CHttpReq& operator=(const CHttpReq& r);
    ~CHttpReq();

    void close();
    void cancel();

    /* sends initial request to an sepcified url */
    void send(const std::string& url, const std::string& request, std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

    /* sends subsequent request for keep-alive connections */
    void send(const std::string& request);
    
    void on_complete(CompleteCallback cb);
    void on_error(ErrorCallback cb);
    
    void set_read_timeout(std::chrono::milliseconds ms);
    void set_proxy(NETPROXY type, uint64 netaddr, const NETPROXYPARAM* param = nullptr);
    void clear_proxy();
    void resolve_redirect(bool state, int32 depth = 7);
    
    CHttpResponse& response();
    bool is_complete();
    HCONN conn_handle();
    std::string host();
    uint32 errcode();
    
    void wait();
    bool wait(std::chrono::milliseconds ms);

    /* dumps any completed http request into disk file in the dir with app exe */
    void dump_on_complete(bool state);
    
private:
    context& ctx();

private:
    context* m_pContext;
};