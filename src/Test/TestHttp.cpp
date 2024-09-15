#include "TestWebsocket.hpp"
#include "TestUserAgent.hpp"

#include "Shared/Network/Net.hpp"
#include "Shared/Common/UserAgent.hpp"

#include "Utils/Http/HttpReq.hpp"
#include "Utils/Base64/Base64.hpp"
#include "Utils/Misc/WebUtils.hpp"


static void TestHTTP_OK()
{
    CHttpReq rq;
    rq.on_complete([&](CHttpReq& rq, CHttpResponse& resp) {
        ASSERT(rq.is_complete() == true);
        ASSERT(resp.is_keepalive() == false);
        ASSERT(resp.status() == httpstatus::code_ok);
        return true;
    });
    rq.on_error([&](CHttpReq& req, int32 errcode) {
        ASSERT(false);
    });
    rq.send(
        "https://www.google.com",
        "GET / HTTP/1.1\r\n"
        "Host: www.google.com\r\n"
        "Connection: close\r\n"
        "User-Agent: " + TestUserAgent + "\r\n"
        "\r\n"
    );
    rq.wait();
};


static void TestHTTP_KeepAlive()
{
    int32 step = 0;
    
    CHttpReq rq;
    rq.on_complete([&](CHttpReq& rq, CHttpResponse& resp) {
        if (step == 0) {
            ASSERT(rq.is_complete() == false);   // keepalive response
            ASSERT(resp.is_keepalive() == true);
            ++step;
            return false;                   // return false to dont continue http stream and disconnect
        }
        else {
            ASSERT(rq.is_complete() == true);    // connection close event eof should be true
            return true;
        };
    });
    rq.on_error([&](CHttpReq& req, int32 errcode) {
        ASSERT(false);
    });
    rq.send(
        "https://www.google.com",
        "GET / HTTP/1.1\r\n"
        "Host: www.google.com\r\n"
        "Connection: keep-alive\r\n"
        "User-Agent: " + TestUserAgent + "\r\n"
        "\r\n"
    );
    rq.wait();
};


static void TestHTTP_AutoRedirect()
{
    CHttpReq rq;
    rq.on_complete([&](CHttpReq& rq, CHttpResponse& resp) {
        ASSERT(rq.is_complete() == true);
        ASSERT(httpstatus::is_redirect(resp.status()) == false);
        ASSERT(resp.status() == httpstatus::code_ok);
        return true;
    });
    rq.on_error([&](CHttpReq& req, int32 errcode) {
        ASSERT(false);
    });
    rq.send(
        "https://www.google.com",
        "GET / HTTP/1.1\r\n"
        "Host: google.com\r\n"  // should response with move to www.google.com
        "Connection: close\r\n"
        "User-Agent: " + TestUserAgent + "\r\n"
        "\r\n"
    );
    rq.resolve_redirect(true, 1);
    rq.wait();
};


static void TestHTTP_ManualRedirect()
{
    size_t state = 0; // 0 - handle redirect, 1 handle OK
    std::string url = "https://www.google.com";
    std::string request =
        "GET / HTTP/1.1\r\n"
        "Host: google.com\r\n"  // should response with move to www.google.com
        "Connection: close\r\n"
        "User-Agent: " + TestUserAgent + "\r\n"
        "\r\n";

    CHttpReq rq;
    rq.on_complete([&](CHttpReq& rq, CHttpResponse& resp) {
        switch (state++)
        {
        case 0:
            {
                ASSERT(rq.is_complete() == true);
                ASSERT(httpstatus::is_redirect(resp.status()) == true);

                std::string loc = resp.header_value("location");
                ASSERT(loc.empty() == false);

                loc = WebUrlExtractDomain(loc);
                ASSERT(loc.empty() == false);

                httputils::change_request_location(request, loc);

                rq.send(url, request);
            }
            break;

        case 1:
            {
                ASSERT(rq.is_complete() == true);
                ASSERT(httpstatus::is_redirect(resp.status()) == false);
                ASSERT(resp.status() == httpstatus::code_ok);
            }
            break;

        default:
            ASSERT(false);
            break;
        };
        return true;
    });
    rq.on_error([&](CHttpReq& req, int32 errcode) {
        ASSERT(false);
    });
    rq.send(url, request);
    rq.resolve_redirect(false);
    rq.wait();
};


static void TestHTTP_SendToIpPortURL()
{
    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Connection: close\r\n"
        "Content-Length: 0\r\n"
        "User-Agent: " + TestUserAgent + "\r\n"
        "\r\n";

    uint16 port = 8886;
    uint32 my_ips[4] = { 0 };
    int32 my_ips_cnt = COUNT_OF(my_ips);
    if (!NetGetAddrinfoSelf(my_ips, &my_ips_cnt))
        ASSERT(false);

    /* start local server */
    HCONN hSv = NetTcpOpen([&](HCONN hConn, NETEVENT evt, uint32 err, uint64 addr, const void* data, uint32 size, void* param) {        
        switch (evt)
        {
        case NETEVENT_CONNECT:
            break;
        case NETEVENT_RECV:
            NetTcpSend(hConn, response.c_str(), response.length());
            NetTcpClose(hConn);
            break;
        default:
            ASSERT(false);
            break;
        };
        return true;
    });
    if (!NetTcpListen(hSv, 4, my_ips[0], port))
        ASSERT(false);

    /* make http request */
    CHttpReq rq;
    rq.on_complete([&](CHttpReq& rq, CHttpResponse& resp) {
        ASSERT(rq.is_complete() == true);
        ASSERT(httpstatus::is_redirect(resp.status()) == false);
        ASSERT(resp.status() == httpstatus::code_ok);
        return true;
    });
    rq.on_error([&](CHttpReq& req, int32 errcode) {
        ASSERT(false);        
    });

    uint64 netAddr;
    NetAddrInit(&netAddr, my_ips[0], port);    
    rq.send(
        NetAddrToStdString(&netAddr, ':'),
        "GET / HTTP/1.1\r\n"
        "Connection: close\r\n"
        "User-Agent: " + TestUserAgent + "\r\n"
        "\r\n"
    );
    rq.resolve_redirect(true, 2);
    rq.wait();

    /* stop & close server */
    NetTcpClose(hSv);
};


void TestHTTP()
{
    TestHTTP_OK();
    TestHTTP_KeepAlive();
    TestHTTP_AutoRedirect();
    TestHTTP_ManualRedirect();
    TestHTTP_SendToIpPortURL();
};