#include "BngUtil.hpp"
#include "BngSettings.hpp"

#include "Shared/Common/UserAgent.hpp"

#include "Utils/Json/Json.hpp"
#include "Utils/Http/HttpReq.hpp"
#include "Utils/Misc/WebUtils.hpp"


static const char* BNGU_USER_AGENT = nullptr;
static const uint32 BNGU_REQ_TIMEOUT = 10000u;


static const char* BnguReqResolveHost(void)
{
    static bool FlagComplete = false;
    static char EndpointHost[256] = { 0 };

    if (FlagComplete)
        return EndpointHost;
    
    CHttpReq req;
    req.resolve_redirect(true);
    req.set_timeout(BNGU_REQ_TIMEOUT);
    req.set_request(
        "GET / HTTP/1.1\r\n"
        "Host: " + std::string(BngSettings.Host) + "\r\n"
        "User-Agent: " + std::string(BNGU_USER_AGENT) + "\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n"
        "\r\n"
    );
    req.on_complete(
        [&](CHttpReq& req, CHttpResponse& resp)
        {
            if (resp.status() != httpstatus::code_ok)
                return;

            std::strcpy(EndpointHost, req.get_host().c_str());
            FlagComplete = true;
        }
    );
    req.on_error(
        [&](CHttpReq& req, int32 errcode)
        {
            OUTPUTLN("resolve host err: %s", CHttpReq::errcode_to_str(errcode));
        }
    );
    if (req.send("https://" + std::string(BngSettings.Host), BNGU_REQ_TIMEOUT))
        req.wait();

    return EndpointHost;
};


static bool BnguReqCheckLive(const std::string& ChannelName)
{
    bool bResult = false;
    std::string Url = "https://" + std::string(BnguReqResolveHost());
    
    CHttpReq req;
    req.set_timeout(BNGU_REQ_TIMEOUT);
	req.set_request(
		"GET /" + ChannelName + " HTTP/1.1\r\n"
		"Host: " + WebUrlExtractDomain(Url) + "\r\n"
		"Referer: " + Url + "/\r\n"
		"User-Agent: " + std::string(BNGU_USER_AGENT) + "\r\n"
		"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9\r\n"
		"Connection: close\r\n"
		"\r\n"
	);
    req.on_complete(
        [&](CHttpReq& req, CHttpResponse& resp)
        {
            bResult = (resp.status() != httpstatus::code_found);
        }
    );
    if (req.send(Url, BNGU_REQ_TIMEOUT))
        req.wait();

    return bResult;
};


static bool BnguReqCheckExist(const std::string& ChannelName)
{
    bool bResult = false;
	std::string Url = "https://" + std::string(BnguReqResolveHost());

    CHttpReq req;
    req.set_timeout(BNGU_REQ_TIMEOUT);
	req.set_request(
		"GET /" + ChannelName + " HTTP/1.1\r\n"
		"Host: " + WebUrlExtractDomain(Url) + "\r\n"
		"Referer: " + Url + "/\r\n"
		"User-Agent: " + std::string(BNGU_USER_AGENT) + "\r\n"
		"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9\r\n"
		"Connection: close\r\n"
		"\r\n"
	);
    req.on_complete(
        [&](CHttpReq& req, CHttpResponse& resp)
        {
            bResult = (req.response().status() != httpstatus::code_not_found);
        }
    );
    if (req.send(Url, BNGU_REQ_TIMEOUT))
        req.wait();

    return bResult;
};


void BnguInitialize(void)
{
    BNGU_USER_AGENT = UserAgentGenereate();
};


void BnguPreTerminate(void)
{
	;
};


void BnguPostTerminate(void)
{
    ;
};


bool BnguIsChannelExist(const char* ChannelName)
{
    return BnguReqCheckExist(ChannelName);
};


bool BnguIsChannelLive(const char* ChannelName)
{
    return BnguReqCheckLive(ChannelName);
};


int32 BnguGetChannelViewerCount(const char* ChannelName)
{
    //
    //  TODO bng real viewer count check
    //
    return 0;
};


std::string BnguGetHostForMe(void)
{
    return BnguReqResolveHost();
};