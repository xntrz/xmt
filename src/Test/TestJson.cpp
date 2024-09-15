#include "TestWebsocket.hpp"
#include "TestUserAgent.hpp"

#include "Utils/Json/Json.hpp"
#include "Utils/Http/HttpReq.hpp"
#include "Utils/Base64/Base64.hpp"
#include "Utils/Misc/WebUtils.hpp"


void TestJson()
{
    CHttpReq rq;
    rq.on_complete([&](CHttpReq& rq, CHttpResponse& resp) {
        ASSERT(resp.status() == httpstatus::code_ok);
        try {
            CJson json(resp.body(), resp.body_size());
            ASSERT(json["method"].data()            == "GET");
            ASSERT(json["protocol"].data()          == "https");
            ASSERT(json["path"].data()              == "/");
            ASSERT(json["headers"]["Host"].data()   == "echo.free.beeceptor.com");
        }
        catch (const CJson::exception& e) {
			(void)e;
            ASSERT(false);
        };
        return true;
    });
    rq.on_error([&](CHttpReq& req, int32 errcode) {
        ASSERT(false);
    });
    rq.send(
        "https://echo.free.beeceptor.com/",
        "GET / HTTP/1.1\r\n"
        "Host: echo.free.beeceptor.com\r\n"
        "Connection: close\r\n"
        "User-Agent: " + TestUserAgent + "\r\n"
        "\r\n"
    );
    rq.wait();
};