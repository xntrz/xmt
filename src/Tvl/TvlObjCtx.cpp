#include "TvlObjCtx.hpp"
#include "TvlUtil.hpp"
#include "TvlSettings.hpp"
#include "TvlResult.hpp"

#include "Utils/Misc/WebUtils.hpp"
#include "Utils/Json/Json.hpp"
#include "Utils/Base64/Base64.hpp"

#include "Shared/Network/Net.hpp"
#include "Shared/Common/UserAgent.hpp"
#include "Shared/Common/Time.hpp"
#include "Shared/Common/Random.hpp"


namespace TVLCTX
{
    const uint32 HTTP_TIMEOUT = 5000;
    const uint32 WS_TIMEOUT = 60000;

    enum ASYNCSTATUS
    {
        ASYNCSTATUS_NONE = 0,
        ASYNCSTATUS_OK,
        ASYNCSTATUS_FAIL,
    };

    enum STATE
    {
        STATE_WATCH = 0,
        STATE_WS_TICKET,
        STATE_WS_TOKEN,
        STATE_WS,
    };

    enum SVCFLAG
    {
        SVCFLAG_RUN         = BIT(0),
        SVCFLAG_SEQ_NEXT    = BIT(1),
        SVCFLAG_SEQ_RST     = BIT(2),
        SVCFLAG_EOL         = BIT(3),
    };

    namespace DB
    {
        const char* USERAGENT   = "db_useragent";
        const char* KEPLER      = "db_kepler";
        const char* TOKEN       = "db_token";
        const char* TICKET      = "db_ticket";
        const char* PVID        = "db_pvid";
        const char* TID         = "db_tid";
        const char* QID         = "db_qid";
    };

    enum WSOP
    {
        WSOP_UNKOWN                 = 0,
        WSOP_SUBSCRIBE              = 1,
        WSOP_SUBSCRIBE_ACK          = 2,
        WSOP_RAWMSG                 = 3,
        WSOP_RAWMSG_ACK             = 4,
        WSOP_SEND_MSG               = 5,
        WSOP_SEND_MSG_ACK           = 6,
        WSOP_HEART_BEAT             = 7,
        WSOP_HEART_BEAT_ACK         = 8,
        WSOP_CANCEL_SUBSCRIBE       = 9,
        WSOP_CANCEL_SUBSCRIBE_ACK   = 10,
        WSOP_SET_FILTER             = 11,
        WSOP_SET_FILTER_ACK         = 12,
        WSOP_SET_OPTIONS            = 13,
        WSOP_SET_OPTIONS_ACK        = 14,
        WSOP_FORCE_KICK_OFF         = 254,
        WSOP_TEST                   = 255,
    };

    enum WSMSG
    {
        WSMSG_DEFAULT               = 0,
        WSMSG_BULLET_CHAT           = 1,
    };

    enum WATCHSTATUS
    {
        WATCHSTATUS_DEFAULT         = 0,
        WATCHSTATUS_WATCHING        = 1,
        WATCHSTATUS_NOT_WATCHING    = 2,
    };

#pragma pack(push, 1)    
    struct PACKET_HDR
    {
        int32 MsgSize;
        int16 HdrSize;
        int16 Unk0;     // always 1
        int16 Opcode;
        int32 SequenceNo;
        int16 Unk1;     // always 0
        int16 Unk2;     // always 0
    };
#pragma pack(pop)

    static_assert(sizeof(PACKET_HDR) == 18, "update me");
};


//
//  Tvl state base obj
//
void CTvlStateBase::Observing(void)
{
    if (GetAsyncStatus() != TVLCTX::ASYNCSTATUS_NONE)
    {
        Subject().ServiceRequest(
            (GetAsyncStatus() == TVLCTX::ASYNCSTATUS_OK) ?
            (TVLCTX::SVCFLAG_SEQ_NEXT)
            :
            (TVLCTX::SVCFLAG_SEQ_NEXT | TVLCTX::SVCFLAG_SEQ_RST)
        );
    };    
};


void CTvlStateBase::SetAsyncStatus(int32 iAsyncStatus)
{
    Shared().m_iAsyncStatus.store(iAsyncStatus);
};


int32 CTvlStateBase::GetAsyncStatus(void) 
{
    return Shared().m_iAsyncStatus.load();
};


CTvlObjCtxShared& CTvlStateBase::Shared(void)
{
    return *((CTvlObjCtxShared*)Subject().GetShared());
};


CHttpReq& CTvlStateBase::Req(void)
{
    return Shared().m_req;
};


CDataStore& CTvlStateBase::Ds(void)
{
    return Shared().m_DataStore;
};


CWebsocket& CTvlStateBase::Ws(void)
{
    return Shared().m_ws;
};


//
//  Tvl video resource watch state
//
void CTvlStateWatch::Attach(void* Param)
{
    //
    //  TODO each vod or clip limited by 1000 views per day from one ip
    //
    const char* QueryFormat = "chunk=1&reqms=%s&qid=%s&cli=4&client_info={%%22device_info%%22:%%22{\\%%22tid\\%%22:\\%%22%s\\%%22}%%22}&resolution=824*1536";
    const char* PayloadFormat = "[{\"operationName\":\"vod_VodResourceService_AddWatchNum\",\"variables\":{\"params\":{\"vid\":\"%s\"}},\"extensions\":{}}]";
        
    char Query[4096];
    Query[0] = '\0';

    char Payload[4096];
    Payload[0] = '\0';

    int32 QueryLen = sprintf_s(
        Query,
        sizeof(Query),
        QueryFormat,
        std::to_string(TimeCurrentUnix64()).c_str(),
        Ds().Get(TVLCTX::DB::QID).c_str(),
        Ds().Get(TVLCTX::DB::TID).c_str()
    );

    int32 PayloadLen = sprintf_s(Payload, sizeof(Payload), PayloadFormat, TvlSettings.TargetId);

    Req().set_timeout(TVLCTX::HTTP_TIMEOUT);
    Req().set_request(
        "POST /graphql?" + std::string(Query) + " HTTP/1.1\r\n"
        "Host: api-web.trovo.live\r\n"
        "Referer: https://trovo.live/\r\n"
        "User-Agent: " + Ds().Get(TVLCTX::DB::USERAGENT) + "\r\n"
        "Accept: */*\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: " + std::to_string(PayloadLen) + "\r\n"
        "Connection: close\r\n"
        "\r\n"
        + Payload
    );
    Req().on_error(
        [&](CHttpReq& req, int32 errcode)
        {
            SetAsyncStatus(TVLCTX::ASYNCSTATUS_FAIL);
        }
    );
    Req().on_complete(
        [&](CHttpReq& req, CHttpResponse& resp)
        {
            if (resp.status() != httpstatus::code_ok)
            {
                OUTPUTLN("req failed with statuscode %d", resp.status());
                SetAsyncStatus(TVLCTX::ASYNCSTATUS_FAIL);
                return;
            };

            CTvlResult::Instance().AddViewerCountWork(1);

            //
            //  Force fail status for reset sequence and ctx
            //
            SetAsyncStatus(TVLCTX::ASYNCSTATUS_FAIL);
        }
    );
    Req().send("https://api-web.trovo.live");
};


//
//  Tvl ws ticket state
//
void CTvlStateWsTicket::Attach(void* Param)
{
    const char* Payload = "{\"app_info\":{\"version_code\":0,\"version_name\":\"9.9.1\",\"platform\":4,\"terminal_type\":2,\"pvid\":\"%s\",\"language\":\"en-US\",\"client_type\":4,\"platformLanguage\":\"ru-RU\"},\"client_info\":{\"device_info\":\"{\\\"tid\\\":\\\"%s\\\"}\"}}";

    char PayloadFormatted[4096];
    PayloadFormatted[0] = '\0';

    std::string Trash = "{\"data\":\"B3SdDSQV3p-dnbr1p4CjSiDD6WRGY-ssTmtv9mMfSkBpL4FiDiJC8PeDw3NYnWUwmG2Ynp2dhXSYbZienZ2ZQwfy4AdQkyL3s4WV6NAmVHJmgXk5H_-tivGaJztAq0eTGSpMzz29gVOzwlZ_-AdjN1DzMwSQGce_NNC7llnTMlY9_uQh4zrd3ilhQLMi_60HlnsfJP099EWBCzrCKERQqC5IqHADVI3dW3wtsaELXPF8b5CKkwr0fMFzy4saVK3GUMFWSB0fU74uFe_HDln5DXgpuxRUfSzx4VZaeL6Ihh5I_UmprLCyEnSu_7H7AuL2woXhYzDg_LF_EYUWON1Vr8eFyg4Q_ivBc_uDQv8ohzP7C1D9e2xx6wYIdgfrWv24Mio5yJMkDp6diZ2dnZ3NyDcsGIadhcBjKyLcdq8BwAueHRXyKKxQC74G-q0B5NsbDP5576XRA3xVL2zlixPUffkpxfD7Dkztq8cxqy5U9b2rhrMbBgTyLAFRwYYG_CUspLsWXPcsLeWBPhRaj6yEUJg-UPUqB3HLFkT9qkdfbGCjLlY9OeZxl5ydZWKcnZ2d3fWpxfPBg35VLAGw2wMY7qnGs8pbDOykh_Pxg1K__STBswpWXPUrBDH7XlDxtyrHBaFTCFCwRqWB80xfKMe2wRZUx38H5AtWWPcvxaV6_yJa_KvNBhEyfBXZO4iiyqo0--7p577HBlgprgsod1ByPyC0oU4uxXBh4C4z92uBr8IbdkRJ5DC74CgYePjdnZ2d3Z2dnZ2dnZ2dnZ0*\"}";

    int32 Len = sprintf_s(
        PayloadFormatted,
        sizeof(PayloadFormatted),
        Payload,
        Ds().Get(TVLCTX::DB::PVID).c_str(),
        Ds().Get(TVLCTX::DB::TID).c_str()
    );

    Req().set_timeout(TVLCTX::HTTP_TIMEOUT);
    Req().set_request(
        "POST /cgi/rpc/security/WebTicketService/GetTicket?crf=5381&comm=" + CBase64::Encode(PayloadFormatted) + " HTTP/1.1\r\n"
        "Host: api.trovo.live\r\n"
        "Accept: */*\r\n"
        "Referer: https://trovo.live/\r\n"
        "Content-Type: text/plain;charset=UTF-8\r\n"
        "User-Agent: " + Ds().Get(TVLCTX::DB::USERAGENT) + "\r\n"
        "Content-Length: " + std::to_string(Trash.length()) + "\r\n"
        "Cookie: pgg_pvid=" + Ds().Get(TVLCTX::DB::PVID) + "\r\n"
        "\r\n"
        + Trash
    );
    Req().on_error(
        [&](CHttpReq& req, int32 errcode)
        {
            SetAsyncStatus(TVLCTX::ASYNCSTATUS_FAIL);
        }
    );
    Req().on_complete(
        [&](CHttpReq& req, CHttpResponse& resp)
        {
            if (resp.status() != httpstatus::code_ok)
            {
                OUTPUTLN("req failed with statuscode %d", resp.status());
                SetAsyncStatus(TVLCTX::ASYNCSTATUS_FAIL);
                return;
            };

            bool bResult = false;

            try
            {
                CJson json(resp.body(), resp.body_size());

				ASSERT(Ds().GetCount(TVLCTX::DB::TICKET) == 0);
                Ds().Set(TVLCTX::DB::TICKET, json["body"]["ticket"].data());

                ASSERT(Ds().Get(TVLCTX::DB::TICKET).size());

                bResult = true;
            }
            catch (CJson::exception& e)
            {
                REF(e);
                OUTPUTLN("json --> %s", e.what());
            };

            SetAsyncStatus(bResult ? TVLCTX::ASYNCSTATUS_OK : TVLCTX::ASYNCSTATUS_FAIL);
        }
    );
    Req().send("https://api.trovo.live", TVLCTX::HTTP_TIMEOUT);
};


//
//  Tvl ws token state
//
void CTvlStateWsToken::Attach(void* Param)
{
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
        TvluGenerateQID().c_str(),
        TvluGenerateTID().c_str(),
        TvlSettings.TargetId
    );

    int32 PayloadLen = sprintf_s(
        Payload,
        sizeof(Payload),
        PayloadFormat,
        std::to_string(CTvlObjCtx::TargetChannelId).c_str(),
        std::to_string(CTvlObjCtx::TargetChannelId).c_str(),
        Ds().Get(TVLCTX::DB::TICKET).c_str()
    );
    
    Req().set_timeout(TVLCTX::HTTP_TIMEOUT);
    Req().set_request(
        "POST /graphql?" + std::string(Query) + " HTTP/1.1\r\n"
        "Host: api.trovo.live\r\n"
        "Referer: https://trovo.live/\r\n"
        "User-Agent: " + Ds().Get(TVLCTX::DB::USERAGENT) + "\r\n"
        "Accept: */*\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: " + std::to_string(PayloadLen) + "\r\n"
        "Cookie: pgg_pvid=" + Ds().Get(TVLCTX::DB::PVID) + "\r\n"
        "Connection: close\r\n"
        "\r\n"
        + Payload
    );
    Req().on_error(
        [&](CHttpReq& req, int32 errcode)
        {
            SetAsyncStatus(TVLCTX::ASYNCSTATUS_FAIL);
        }
    );
    Req().on_complete(
        [&](CHttpReq& req, CHttpResponse& resp)
        {
            if (resp.status() != httpstatus::code_ok)
            {
                OUTPUTLN("req failed with statuscode %d", resp.status());
                SetAsyncStatus(TVLCTX::ASYNCSTATUS_FAIL);
                return;
            };

            bool bResult = false;

            try
            {
                CJson json(resp.body(), resp.body_size());

				ASSERT(Ds().GetCount(TVLCTX::DB::TOKEN) == 0);
                Ds().Set(TVLCTX::DB::TOKEN, json[0]["data"]["ws_WSTokenService_GetToken"]["token"].data());

                ASSERT(Ds().Get(TVLCTX::DB::TOKEN).size());

                bResult = true;
            }
            catch (CJson::exception& e)
            {
                REF(e);
                OUTPUTLN("json --> %s", e.what());
            };

            SetAsyncStatus(bResult ? TVLCTX::ASYNCSTATUS_OK : TVLCTX::ASYNCSTATUS_FAIL);
        }
    );
    Req().send("https://api.trovo.live", TVLCTX::HTTP_TIMEOUT);
};


//
//  Tvl ws chat state
//
void CTvlStateWs::Attach(void* Param)
{
    m_bBotFlag = false;
    m_PingTimer.SetAlarm(20000 + RndUInt32(2500, 7500));
    m_PingTimer.Reset();

    Ws().on_error(
        [&](CWebsocket& ws, int32 errcode)
        {
            OUTPUTLN("%s", CWebsocket::errcode_to_str(errcode));
            SetAsyncStatus(TVLCTX::ASYNCSTATUS_FAIL);
        }
    );
    Ws().on_connect(
        [&](CWebsocket& ws)
        {
            ws.set_timeout(TVLCTX::WS_TIMEOUT);
            
            std::string Token = Ds().Get(TVLCTX::DB::TOKEN);
            std::vector<unsigned char> Input;

            //
            //  Packet header + payload header + payload
            //
            Input.resize(sizeof(TVLCTX::PACKET_HDR) + 3 + Token.size());

            TVLCTX::PACKET_HDR* Hdr = (TVLCTX::PACKET_HDR*)&Input[0];
            Hdr->MsgSize    = SWAP4(Input.size());
            Hdr->HdrSize    = SWAP2(sizeof(TVLCTX::PACKET_HDR));
            Hdr->Unk0       = SWAP2(1);
            Hdr->Opcode     = SWAP2(TVLCTX::WSOP_SUBSCRIBE);
            Hdr->SequenceNo = SWAP4(1);

            *(uint8*)&Input[sizeof(TVLCTX::PACKET_HDR) + 0] = uint8(0x0A);
            *(uint8*)&Input[sizeof(TVLCTX::PACKET_HDR) + 1] = uint8((127 & Token.size()) | 128);
            *(uint8*)&Input[sizeof(TVLCTX::PACKET_HDR) + 2] = uint8(Token.size() >> 7);

            std::memcpy(&Input[sizeof(TVLCTX::PACKET_HDR) + 3], &Token[0], Token.size());

            ws.send_bin((const char*)&Input[0], Input.size());
        }
    );
    Ws().on_read(
        [&](CWebsocket& ws, const char* data, uint32 datalen, bool binary)
        {            
            if (!binary)
            {
                OUTPUTLN("unexpected data format");
                ws.close();
                return;
            };

            if (datalen < sizeof(TVLCTX::PACKET_HDR))
            {
                OUTPUTLN("unexpected packet size");
                ws.close();
                return;
            };
            
            TVLCTX::PACKET_HDR* Hdr = (TVLCTX::PACKET_HDR*)&data[0];
            TVLCTX::WSOP wsop = TVLCTX::WSOP(SWAP2(Hdr->Opcode));

            switch (wsop)
            {
            case TVLCTX::WSOP_SUBSCRIBE_ACK:
                {
                    m_bBotFlag = true;
                    CTvlResult::Instance().AddViewerCountWork(1);
                    
                    SendPing();
                }
                break;

            case TVLCTX::WSOP_HEART_BEAT_ACK:
                {
                    switch (TvlSettings.RunMode)
                    {
                    case TvlRunMode_Keepalive:
                        {
                            ;
                        }
                        break;

                    case TvlRunMode_Rst:
                        {
                            ws.shutdown();
                            SetAsyncStatus(TVLCTX::ASYNCSTATUS_FAIL);
                        }
                        break;

                    case TvlRunMode_TimeoutTest:
                        {
                            ws.shutdown();
                            Subject().ServiceRequest(TVLCTX::SVCFLAG_EOL);
                        }
                        break;

                    default:
                        {
                            ASSERT(false);
                        }
                        break;
                    };
                }
                break;
            };
        }
    );
    Ws().on_disconnect(
        [&](CWebsocket& ws, CWebsocket::closecode closecode)
        {
            if (m_bBotFlag)
                CTvlResult::Instance().AddViewerCountWork(-1);
            
            if (closecode != CWebsocket::closecode_normal)
                OUTPUTLN("unexpected closecode %d - %s", closecode, CWebsocket::closecode_to_str(closecode));

            //
            //  Force fail status for reset sequence
            //
            SetAsyncStatus(TVLCTX::ASYNCSTATUS_FAIL);
        }
    );
    Ws().set_request(
        "GET /sub HTTP/1.1\r\n"
        "Host: chat.trovo.live\r\n"
        "User-Agent: " + Ds().Get(TVLCTX::DB::USERAGENT) + "\r\n"
        "Accept: */*\r\n"
        "Origin: https://trovo.live\r\n"	// RFC 6455, page 18, 8
        "Upgrade: websocket\r\n"
        "Sec-WebSocket-Extensions: permessage-deflate; client_max_window_bits\r\n"
        "Sec-WebSocket-Key: " + CBase64::Encode(WebRndHexString(16)) + "\r\n" // RFC 6455, page 17, 7
        "Sec-WebSocket-Version: 13\r\n"
        "Connection: keep-alive, Upgrade\r\n"
        "\r\n"
    );
    Ws().set_compress(false);
    Ws().set_masking(true);
    Ws().set_timeout(TVLCTX::HTTP_TIMEOUT);
    if (!Ws().connect("wss://chat.trovo.live", TVLCTX::HTTP_TIMEOUT))
    {
        OUTPUTLN("ws connect failed");
        SetAsyncStatus(TVLCTX::ASYNCSTATUS_FAIL);
    };
};


void CTvlStateWs::Detach(void)
{
    ;
};


void CTvlStateWs::Observing(void)
{
	m_PingTimer.Period();

    if (m_PingTimer.IsAlarm())
    {
        m_PingTimer.Reset();
        SendPing();
    };
    
    CTvlStateBase::Observing();
};


void CTvlStateWs::SendPing(void)
{
    std::vector<unsigned char> Input;

    //
    //  Packet header + heartbeat header + heartbeat payload
    //
    Input.resize(sizeof(TVLCTX::PACKET_HDR) + 2);

    TVLCTX::PACKET_HDR* Hdr = (TVLCTX::PACKET_HDR*)&Input[0];
    Hdr->MsgSize    = SWAP4(Input.size());
    Hdr->HdrSize    = SWAP2(sizeof(TVLCTX::PACKET_HDR));
    Hdr->Unk0       = SWAP2(1);
    Hdr->Opcode     = SWAP2(TVLCTX::WSOP_HEART_BEAT);
	Hdr->SequenceNo = 0;    // always 0 in heartbeat packet type

    *(uint8*)&Input[sizeof(TVLCTX::PACKET_HDR) + 0] = uint8(0x10);
    *(uint8*)&Input[sizeof(TVLCTX::PACKET_HDR) + 1] = uint8(TVLCTX::WATCHSTATUS_WATCHING);

    Ws().send_bin((const char*)&Input[0], Input.size());
};


//
//  Tvl ctx obj
//
/*static*/ int32 CTvlObjCtx::TargetChannelId = -1;


CTvlObjCtx::CTvlObjCtx(void)
: m_StateWatch()
, m_StateWsTicket()
, m_StateWsToken()
, m_StateWs()
, m_Shared()
, m_nStateSequenceCur(STATE_LABEL_EOL)
, m_nStateSequenceHead(STATE_LABEL_EOL)
, m_nStateSequenceTail(STATE_LABEL_EOL)
, m_nStateSequenceSave(STATE_LABEL_EOL)
, m_bInitFlag(false)
, m_bBotFlag(false)
{
    SetShared(&m_Shared);
};


CTvlObjCtx::~CTvlObjCtx(void)
{
    ;
};


void CTvlObjCtx::Start(void)
{
    if (!m_bInitFlag)
    {
        m_bInitFlag = true;
        m_Shared.m_DataStore.Set(TVLCTX::DB::USERAGENT, UserAgentGenereate());
        if (TvlSettings.BotType == TvlBotType_Stream)
        {
            m_Shared.m_DataStore.Set(TVLCTX::DB::PVID, TvluGeneratePVID());
            m_Shared.m_DataStore.Set(TVLCTX::DB::TID, TvluGenerateTID());

            StateRegist(TVLCTX::STATE_WS_TICKET, &m_StateWsTicket, false);
            StateRegist(TVLCTX::STATE_WS_TOKEN, &m_StateWsToken, false);
            StateRegist(TVLCTX::STATE_WS, &m_StateWs, false);
            m_nStateSequenceHead = TVLCTX::STATE_WS_TICKET;
            m_nStateSequenceTail = TVLCTX::STATE_WS;
        }
        else
        {
            m_Shared.m_DataStore.Set(TVLCTX::DB::QID, TvluGenerateQID());
            m_Shared.m_DataStore.Set(TVLCTX::DB::TID, TvluGenerateTID());

            StateRegist(TVLCTX::STATE_WATCH, &m_StateWatch, false);
            m_nStateSequenceHead = TVLCTX::STATE_WATCH;
            m_nStateSequenceTail = TVLCTX::STATE_WATCH;
        };
        m_nStateSequenceCur = m_nStateSequenceHead;
        m_nStateSequenceSave = STATE_LABEL_EOL;

        ServiceRequest(TVLCTX::SVCFLAG_SEQ_NEXT);
    };
};


void CTvlObjCtx::Stop(void)
{
    if (TvlSettings.BotType == TvlBotType_Stream)
        m_Shared.m_ws.shutdown();
};


void CTvlObjCtx::Service(uint32 uServiceFlags)
{
    if (IS_FLAG_SET(uServiceFlags, TVLCTX::SVCFLAG_EOL))
    {
        StateJump(STATE_LABEL_EOL);
        return;
    };
    
    if (IS_FLAG_SET(uServiceFlags, TVLCTX::SVCFLAG_SEQ_RST))
	{
		m_Shared.m_DataStore.Clear();
        m_Shared.m_DataStore.Set(TVLCTX::DB::USERAGENT, UserAgentGenereate());
        if (TvlSettings.BotType == TvlBotType_Stream)
        {
            m_Shared.m_DataStore.Set(TVLCTX::DB::PVID, TvluGeneratePVID());
            m_Shared.m_DataStore.Set(TVLCTX::DB::TID, TvluGenerateTID());
            m_Shared.m_ws.shutdown();
        }
        else
        {
            m_Shared.m_DataStore.Set(TVLCTX::DB::QID, TvluGenerateQID());
            m_Shared.m_DataStore.Set(TVLCTX::DB::TID, TvluGenerateTID());
        };
            
        m_nStateSequenceCur = m_nStateSequenceHead;        
    };

    if (IS_FLAG_SET(uServiceFlags, TVLCTX::SVCFLAG_SEQ_NEXT))
    {
        m_Shared.m_iAsyncStatus = TVLCTX::ASYNCSTATUS_NONE;
        StateJump(m_nStateSequenceCur);
        m_nStateSequenceCur = Clamp(m_nStateSequenceCur + 1, m_nStateSequenceHead, m_nStateSequenceTail);
    };
};