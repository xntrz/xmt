#include "TvlUtil.hpp"
#include "TvlSettings.hpp"

#include "Shared/Common/UserAgent.hpp"
#include "Shared/Common/Random.hpp"
#include "Shared/Common/Time.hpp"

#include "Utils/Json/Json.hpp"
#include "Utils/Http/HttpReq.hpp"
#include "Utils/Http/HttpStatus.hpp"
#include "Utils/Misc/WebUtils.hpp"


static const char* TVLU_USER_AGENT = nullptr;


union TvluResult_t
{
    int32 Integer;
    bool Boolean;
};


enum TvluVodCheckType_t
{
    TvluVodCheckType_Exist = 0,
    TvluVodCheckType_Views,
};


enum TvluChannelCheckType_t
{
    TvluChannelCheckType_Exist = 0,
    TvluChannelCheckType_Live,
    TvluChannelCheckType_Viewers,
    TvluChannelCheckType_ChannelId,
};


static TvluResult_t TvluReqVodCheck(const char* VodId, TvluVodCheckType_t Checktype)
{
    TvluResult_t Result = {};
    if (Checktype == TvluVodCheckType_Views)
        Result.Integer = -1;
    
    const char* QueryFormat = "chunk=1&reqms=%s&qid=%s&cli=4&client_info={%%22device_info%%22:%%22{\\%%22tid\\%%22:\\%%22%s\\%%22}%%22}&resolution=824*1536";
    const char* PayloadFormat = "[{\"operationName\":\"vod_VodReaderService_BatchGetVodDetailInfo\",\"variables\":{\"params\":{\"vids\":[\"%s\"]}},\"extensions\":{}}]";

    char Query[4096];
    Query[0] = '\0';

    char Payload[4096];
    Payload[0] = '\0';

    int32 QueryLen = sprintf_s(
        Query,
        sizeof(Query),
        QueryFormat,
        std::to_string(TimeCurrentUnix64()).c_str(),
        TvluGenerateQID().c_str(),
        TvluGenerateTID().c_str()
    );

    int32 PayloadLen = sprintf_s(
        Payload,
        sizeof(Payload),
        PayloadFormat,
        VodId
    );

    CHttpReq req;
    req.set_timeout(5000);
    req.set_request(
        "POST /graphql?" + std::string(Query) + " HTTP/1.1\r\n"
        "Host: api-web.trovo.live\r\n"
        "origin: https://trovo.live\r\n"
        "Referer: https://trovo.live/\r\n"
        "User-Agent: " + std::string(TVLU_USER_AGENT) + "\r\n"
        "Accept: */*\r\n"
        "cookie: pgg_pvid=" + TvluGeneratePVID() + "\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: " + std::to_string(PayloadLen) + "\r\n"
        "Connection: close\r\n"
        "\r\n"
        + Payload
    );
    req.on_error(
        [&](CHttpReq& req, int32 errcode)
        {
            OUTPUTLN("req failed - error: %s", CHttpReq::errcode_to_str(errcode));
        }
    );
    req.on_complete(
        [&](CHttpReq& req, CHttpResponse& resp)
        {
            if (resp.status() != httpstatus::code_ok)
            {
                OUTPUTLN("req failed - return code: %d", resp.status());
                return;
            };

            try
            {
                CJson json(req.response().body(), req.response().body_size());
                if (json.is_array())
                {
                    switch (Checktype)
                    {
                    case TvluVodCheckType_Exist:
                        {
                            try
                            {
                                //
                                //  Accessing data without throwing exception means vod is founded!
                                //
                                std::string Data = json[0]["data"]["vod_VodReaderService_BatchGetVodDetailInfo"]["VodDetailInfos"][VodId]["channelInfo"].data();
                                Result.Boolean = true;
                            }
                            catch (CJson::exception& e)
                            {
                                REF(e);
                                Result.Boolean = false;
                            };
                        }
                        break;

                    case TvluVodCheckType_Views:
                        {
                            std::string Data = json[0]["data"]["vod_VodReaderService_BatchGetVodDetailInfo"]["VodDetailInfos"][VodId]["vodInfo"]["watchNum"].data();
                            Result.Integer = std::atoi(Data.c_str());
                        }
                        break;

                    default:
                        ASSERT(false);
                        break;
                    };
                };
            }
            catch (CJson::exception& e)
            {
                REF(e);
                OUTPUTLN("json --> %s", e.what());
            };
        }
    );
    if (req.send("https://api-web.trovo.live", 5000u))
        req.wait();

    return Result;
};


static TvluResult_t TvluReqChannelCheck(const std::string& ChannelName, TvluChannelCheckType_t Checktype)
{
    TvluResult_t Result = {};
    if (Checktype == TvluChannelCheckType_ChannelId ||
        Checktype == TvluChannelCheckType_Viewers)
        Result.Integer = -1;

   	const char* QueryFormat = "chunk=1&reqms=%s&qid=%s&cli=4&client_info={%%22device_info%%22:%%22{\\%%22tid\\%%22:\\%%22%s\\%%22}%%22}&from=%%2Fs%%2F%s&locale=RU&resolution=824*1536";
    const char* PayloadFormat = "[{\"operationName\":\"live_LiveReaderService_GetLiveInfo\",\"variables\":{\"params\":{\"userName\":\"%s\",\"requireDecorations\":true}},\"extensions\":{}}]";
    
    char Query[4096];
    Query[0] = '\0';

    char Payload[4096];
    Payload[0] = '\0';

    int32 QueryLen = sprintf_s(
        Query,
        sizeof(Query),
        QueryFormat,
        std::to_string(TimeCurrentUnix64()).c_str(),
        TvluGenerateQID().c_str(), 
        TvluGenerateTID().c_str(),
        ChannelName.c_str()
    );

    int32 PayloadLen = sprintf_s(
        Payload,
        sizeof(Payload),
        PayloadFormat,
        ChannelName.c_str()
    );

    CHttpReq req;
    req.set_timeout(5000);
    req.set_request(
        "POST /graphql?" + std::string(Query) + " HTTP/1.1\r\n"
        "Host: api-web.trovo.live\r\n"
		"origin: https://trovo.live\r\n"
        "Referer: https://trovo.live/\r\n"
        "User-Agent: " + std::string(TVLU_USER_AGENT) + "\r\n"
        "Accept: */*\r\n"
		"cookie: pgg_pvid=" + TvluGeneratePVID() + "\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: " + std::to_string(PayloadLen) + "\r\n"
        "Connection: close\r\n"
        "\r\n"
        + Payload
    );
    req.on_error(
        [&](CHttpReq& req, int32 errcode)
        {
            OUTPUTLN("req failed - error: %s", CHttpReq::errcode_to_str(errcode));
        }
    );    
    req.on_complete(
        [&](CHttpReq& req, CHttpResponse& resp)
        {
            if (resp.status() != httpstatus::code_ok)
            {
                OUTPUTLN("req failed - return code: %d", resp.status());
                return;
            };

            try
            {
                CJson json(req.response().body(), req.response().body_size());
                if (json.is_array())
                {
                    switch (Checktype)
                    {
                    case TvluChannelCheckType_Exist:
                        {
                            try
                            {
                                //
                                //  Accessing data without throwing exception means error is present
                                //
                                std::string Data = json[0]["errors"][0]["message"].data();
                                Result.Boolean = false;
                            }
                            catch (CJson::exception& e)
                            {
                                REF(e);
                                Result.Boolean = true;
                            };
                        }
                        break;

                    case TvluChannelCheckType_Live:
                        {
                            std::string Data = json[0]["data"]["live_LiveReaderService_GetLiveInfo"]["isLive"].data();
                            Result.Boolean = (std::atoi(Data.c_str()) == 1);
                        }
                        break;

                    case TvluChannelCheckType_Viewers:
                        {
                            std::string Data = json[0]["data"]["live_LiveReaderService_GetLiveInfo"]["channelInfo"]["viewers"].data();
                            Result.Integer = std::atoi(Data.c_str());
                        }
                        break;

                    case TvluChannelCheckType_ChannelId:
                        {
                            std::string Data = json[0]["data"]["live_LiveReaderService_GetLiveInfo"]["channelInfo"]["id"].data();
                            Result.Integer = std::atoi(Data.c_str());
                        }
                        break;

                    default:
                        ASSERT(false);
                        break;
                    };
                };
            }
            catch (CJson::exception& e)
            {
                REF(e);
                OUTPUTLN("json --> %s", e.what());
            };
        }
    );
    if (req.send("https://api-web.trovo.live", 5000u))
        req.wait();

    return Result;
};


void TvluInitialize(void)
{
    TVLU_USER_AGENT = UserAgentGenereate();
};


void TvluPreTerminate(void)
{
    CHttpReq::cancel_all();
};


void TvluPostTerminate(void)
{
    ;
};


bool TvluIsChannelExist(const char* ChannelName)
{
    return TvluReqChannelCheck(ChannelName, TvluChannelCheckType_Exist).Boolean;
};


bool TvluIsChannelLive(const char* ChannelName)
{
    return TvluReqChannelCheck(ChannelName, TvluChannelCheckType_Live).Boolean;    
};


int32 TvluGetChannelViewersCount(const char* ChannelName)
{
    return TvluReqChannelCheck(ChannelName, TvluChannelCheckType_Viewers).Integer;
};


int32 TvluGetChannelId(const char* ChannelName)
{
    return TvluReqChannelCheck(ChannelName, TvluChannelCheckType_ChannelId).Integer;
};


std::string TvluGeneratePVID(void)
{
    uint32 Hour = 0;
    uint32 Minute = 0;
    uint32 Second = 0;
    uint32 Ms = 0;
    uint32 Year = 0;
    uint32 Month = 0;
    uint32 Day = 0;    
    TimeCurrentLocalEx(&Hour, &Minute, &Second, &Ms, &Year, &Month, &Day);

    std::string t = (Month  < 10 ? "0" : "") + std::to_string(Month);
    std::string n = (Day    < 10 ? "0" : "") + std::to_string(Day);
    std::string o = (Hour   < 10 ? "0" : "") + std::to_string(Hour);
    std::string r = std::to_string(Year % 2000) + t + n + o;
    
    uint64 uTimestamp = TimeCurrentUnix64();
	uTimestamp %= (uint32(1e10) / 1000);

    uint32 uRand = uint32(float(0x7FFFFFFF) * RndReal32(0.0f, 1.0f));
    
    std::string Result = std::to_string(uint64( uint64(uRand) + uTimestamp) ) + r;
    return Result;
};


std::string TvluGenerateTID(void)
{
    const uint32 Begin  = uint32(1e3);  // 1000
    const uint32 End    = uint32(9e3);  // 9000

    uint32 uRand = RndUInt32(Begin, End);
    uint64 uTimestamp = TimeCurrentUnix64();
    
    return std::string(std::to_string(uTimestamp) + std::to_string(uRand));
};


std::string TvluGenerateDeviceId(void)
{
    return TvluGenerateTID();
};


std::string TvluGenerateQID(void)
{
    return WebRndHexString(10, true);
};


bool TvluIsChannelProtected(const char* ChannelName)
{
	//
	//	By default assume channel is protected to raise error if we are will fail at some request stage
	//
	bool bResult = true;

	std::string Tid = TvluGenerateTID();
	std::string Qid = TvluGenerateQID();
	int32 ChannelId = TvluGetChannelId(ChannelName);

	if ((ChannelId == -1) || Tid.empty() || Qid.empty())
		return bResult;

    const char* QueryFormat = "chunk=1&reqms=%s&qid=%s&cli=4&client_info={%%22device_info%%22:%%22{\\%%22tid\\%%22:\\%%22%s\\%%22}%%22}&from=%%2Fs%%2F%s&locale=RU&resolution=824*1536";
    const char* PayloadFormat = "[{\"operationName\":\"ws_WSTokenService_GetToken\",\"variables\":{\"params\":{\"subinfo\":{\"page\":{\"scene\":\"SCENE_CHANNEL\",\"pageID\":%s},\"streamerID\":%s}}},\"extensions\":{\"webTicket\":\"%s\",\"singleReq\":true}}]";

	char Query[4096];
	Query[0] = '\0';

	char Payload[4096];
	Payload[0] = '\0';

    int32 QueryLen = sprintf_s(
        Query,
        sizeof(Query),
        QueryFormat,
        std::to_string(TimeCurrentUnix64()).c_str(),
        Qid.c_str(),
        Tid.c_str(),
		ChannelName
    );

	//
	//	Its fine to pass last random generated parameter instead correct "webticket" to receive response
	//
	int32 PayloadLen = sprintf_s(
		Payload,
		sizeof(Payload),
		PayloadFormat,
		std::to_string(ChannelId).c_str(),
		std::to_string(ChannelId).c_str(),
		WebRndHexString(8, true).c_str()
    );

    CHttpReq req;
    req.set_timeout(5000);
    req.set_request(
        "POST /graphql?" + std::string(Query) + " HTTP/1.1\r\n"
        "Host: api-web.trovo.live\r\n"
        "Referer: https://trovo.live/\r\n"
        "User-Agent: " + std::string(TVLU_USER_AGENT) + "\r\n"
        "Accept: */*\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: " + std::to_string(PayloadLen) + "\r\n"
        "Connection: close\r\n"
        "\r\n"
        + Payload
    );
    req.on_complete(
        [&](CHttpReq& req, CHttpResponse& resp)
        {
            if (resp.status() == httpstatus::code_ok)
            {
                //
                //	If channel is protected returns json with array of errors obj with msg "need login"
                //
                std::string ResponseJson(req.response().body(), req.response().body_size());
                if (ResponseJson.find("need login") == std::string::npos)
                    bResult = false;
            };
        }
    );
    if (req.send("https://api-web.trovo.live"))
        req.wait();

    return bResult;
};


bool TvluIsClipExist(const char* ClipId)
{
    return TvluReqVodCheck(ClipId, TvluVodCheckType_Exist).Boolean;
};


int32 TvluGetClipViewsCount(const char* ClipId)
{
    return TvluReqVodCheck(ClipId, TvluVodCheckType_Views).Integer;
};


bool TvluIsVodExist(const char* VodId)
{
    return TvluReqVodCheck(VodId, TvluVodCheckType_Exist).Boolean;
};


int32 TvluGetVodViewsCount(const char* VodId)
{
    return TvluReqVodCheck(VodId, TvluVodCheckType_Views).Integer;
};