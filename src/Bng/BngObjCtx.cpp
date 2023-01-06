#include "BngObjCtx.hpp"
#include "BngUtil.hpp"
#include "BngSettings.hpp"
#include "BngResult.hpp"
#include "BngDecode.hpp"

#include "Utils/Misc/WebUtils.hpp"
#include "Utils/Json/Json.hpp"
#include "Utils/Base64/Base64.hpp"

#include "Shared/Network/Net.hpp"
#include "Shared/Common/UserAgent.hpp"
#include "Shared/Common/Time.hpp"
#include "Shared/Common/Random.hpp"


namespace BNGCTX
{
    const int32 RETRY_MAX = 3;
    const uint32 HTTP_TIMEOUT = 5000;
    const uint32 WSS_TIMEOUT = 60000 * 2;
    
    enum STATE
    {
        STATE_HOST_RESOLVE = 0,
        STATE_INIT_PHASE1,
        STATE_INIT_PHASE2,
        STATE_CHATPROC,
    };

    enum SVCFLAG
    {
        SVCFLAG_YIELD       = BIT(0),
        SVCFLAG_SEQ_NEXT    = BIT(1),
        SVCFLAG_SEQ_SAVE    = BIT(2),
        SVCFLAG_EOL         = BIT(3),
        SVCFLAG_RETRY       = BIT(4),
    };

    namespace DB
    {
        const char* USERAGENT       = "db_useragent";
        const char* DATAKEY         = "db_datakey";
        const char* MYNAME          = "db_myname";
        const char* MYNAMEDISP      = "db_mynamedisp";
        const char* LOCATION        = "db_loc";
        const char* ISRU            = "db_isru";
        const char* COOKIE          = "db_cookie";
        const char* INITIALSTATE    = "db_init";
        const char* ABSPLITGROUP    = "db_absg";
        const char* WSS             = "db_wss";
        const char* CSRF            = "db_csrf";
        const char* CHATHOST        = "db_chathost";
    };
};


//
//  Bng state base obj
//
CBngObjCtxShared& CBngStateBase::Shared(void)
{
    return *((CBngObjCtxShared*)Subject().GetShared());
};


CHttpReq& CBngStateBase::Req(void)
{
    return *Shared().m_pReq;
};


CDataStore& CBngStateBase::Ds(void)
{
    return Shared().m_DataStore;
};


CWebsocket& CBngStateBase::Ws(void)
{
    return Shared().m_ws;
};


std::string CBngStateBase::HostForMe(void)
{
    if (BngSettings.ProxyUseMode)
        return Shared().m_HostForMe;
    else
        return BnguGetHostForMe();
};


void CBngStateBase::RequestYield(void)
{
    if (BngSettings.ProxyUseMode)
    {
        switch (Subject().CurrentStateLabel())
        {
        case BNGCTX::STATE_HOST_RESOLVE:
            {
                Subject().ServiceRequest(BNGCTX::SVCFLAG_YIELD);
            }
            break;

        default:
            {
                if (Shared().m_RetryNo++ < BngSettings.ProxyRetryMax)
                {
                    Subject().ServiceRequest(BNGCTX::SVCFLAG_RETRY);
                }
                else
                {
                    Shared().m_RetryNo = 0;
                    Subject().ServiceRequest(BNGCTX::SVCFLAG_YIELD);
                };
            }
            break;
        };
    }
    else
    {
        Subject().ServiceRequest(BNGCTX::SVCFLAG_YIELD);
    };
};


//
//  Bng host resolve state
//  Running only in proxy mode
//
void CBngStateHostResolve::Attach(void* Param)
{
    Req().resolve_redirect(true);
    Req().set_timeout(BNGCTX::HTTP_TIMEOUT);
    Req().set_request(
        "GET / HTTP/1.1\r\n"
        "Host: " + std::string(BngSettings.Host) + "\r\n"
        "User-Agent: " + Ds().Get(BNGCTX::DB::USERAGENT) + "\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n"
        "sec-fetch-dest: document\r\n"
        "sec-fetch-mode: navigate\r\n"
        "sec-fetch-site: same-origin\r\n"
        "sec-fetch-user: ?1\r\n"
        "upgrade-insecure-requests: 1\r\n"
        "\r\n"
    );
    Req().on_error(
        [&](CHttpReq& req, int32 errcode)
        {
            if (errcode)
                OUTPUTLN("http req failed with error %d - %s", errcode, CHttpReq::errcode_to_str(errcode));

            RequestYield();
        }
    );
    Req().on_complete(
        [&](CHttpReq& req, CHttpResponse& resp)
        {
            if (!httpstatus::is_success(resp.status()))
            {
                OUTPUTLN("http req failed with status %d (host %s)", resp.status(), req.get_host().c_str());
                RequestYield();
                return;
            };

            Shared().m_HostForMe = req.get_host();
            Subject().ServiceRequest(BNGCTX::SVCFLAG_SEQ_NEXT);
        }
    );
    if (BngSettings.ProxyUseMode)
        Req().set_proxy(Subject().GetProxyType(), Subject().GetProxyAddr());
    if (!Req().send("https://" + std::string(BngSettings.Host), BNGCTX::HTTP_TIMEOUT))
        RequestYield();
};


//
//  Bng main state obj
//
void CBngStateInitPhase1::Attach(void* Param)
{
    Req().set_timeout(BNGCTX::HTTP_TIMEOUT);
    Req().set_request(
        "GET /" + std::string(BngSettings.TargetId) + " HTTP/1.1\r\n"
        "Host: " + HostForMe() + "\r\n"
        "Referer: https://" + HostForMe() + "/\r\n"
        "User-Agent: " + Ds().Get(BNGCTX::DB::USERAGENT) + "\r\n"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9\r\n"
        "Connection: close\r\n"
        "\r\n"
    );
    Req().on_error(
        [&](CHttpReq& req, int32 errcode)
        {
            if (errcode)
                OUTPUTLN("http req failed with error %d - %s", errcode, CHttpReq::errcode_to_str(errcode));
            
            RequestYield();
        }
    );
    Req().on_complete(
        [&](CHttpReq& req, CHttpResponse& resp)
        {
            if (resp.status() != httpstatus::code_ok)
            {
                OUTPUTLN("http req failed with status %d", resp.status());
                RequestYield();
                return;
            };

            std::string Csrf = WebSubstrMem(resp.body(), resp.body_size(), "data-csrf_value=\"", "\"");
            std::string Token = resp.cookie_value("bonga20120608");
            std::string Cookie = "bonga20120608=" + Token;
            std::string InitialState = WebSubstrMem(resp.body(), resp.body_size(), "<script data-type=\"initialState\" data-version=\"1\" type=\"text/plain;charset=UTF-8\">", "</script>");
            InitialState = BngDecodeInitialState(InitialState);
            std::string ABSplitGroup = {};
            std::string WSS = {};

            try
            {
                CJson json(&InitialState[0], InitialState.size());

                ABSplitGroup = json["chatABSplitGroup"].data();
                ABSplitGroup = BngDecodeABSplitGroup(ABSplitGroup);

                WSS = json["urls"]["chat"]["@ws_chat"].data();
                WSS = WebUrlExtractProto(WSS) + "://" + WebUrlExtractDomain(WSS);
            }
            catch (CJson::exception& e)
            {
                REF(e);
                OUTPUTLN("json --> %s", e.what());
            };

            if ((!Token.empty())        &&
                (!Cookie.empty())       &&
                (!InitialState.empty()) &&
                (!ABSplitGroup.empty()) &&
                (!WSS.empty())          &&
                (!Csrf.empty()))
            {
                //OUTPUTLN("%s", InitialState.c_str());                
                Ds().Set(BNGCTX::DB::COOKIE, Cookie);
                Ds().Set(BNGCTX::DB::INITIALSTATE, InitialState);
                Ds().Set(BNGCTX::DB::ABSPLITGROUP, ABSplitGroup);
                Ds().Set(BNGCTX::DB::WSS, WSS);
                Ds().Set(BNGCTX::DB::CSRF, Csrf);

                Subject().ServiceRequest(BNGCTX::SVCFLAG_SEQ_NEXT);
            }
            else
            {
                RequestYield();
            };
        }
    );
    if (BngSettings.ProxyUseMode)
        Req().set_proxy(Subject().GetProxyType(), Subject().GetProxyAddr());
    if (!Req().send("https://" + HostForMe()))
        RequestYield();
};


//
//  Bng playlist state obj
//
void CBngStateInitPhase2::Attach(void* Param)
{
    const char* PayloadFmt = "method=getRoomData&args%%5B%%5D=%s&args%%5B%%5D=&args%%5B%%5D=";
    char Payload[4096];
    Payload[0] = '\0';
    int32 PayloadLen = sprintf_s(
        Payload,
        sizeof(Payload),
        PayloadFmt,
        BngSettings.TargetId
    );

    //const char* PayloadFmt = "method=getRoomData&args%%5B%%5D=%s&args%%5B%%5D=&args%%5B%%5D=&_csrf_token=%s";
    //char Payload[4096];
    //Payload[0] = '\0';
    //int32 PayloadLen = sprintf_s(
    //    Payload,
    //    sizeof(Payload),
    //    PayloadFmt,
    //    BngSettings.TargetId,
    //    Ds().Get(BNGCTX::DB::CSRF).c_str()
    //);

    Req().set_timeout(BNGCTX::HTTP_TIMEOUT);
    Req().set_request(
        "POST /tools/amf.php?res=" + std::to_string(RndUInt32(100000, 999999)) + "&t=" + std::to_string(TimeCurrentUnix64()) + " HTTP/1.1\r\n"
        //"POST /tools/amf.php HTTP/1.1\r\n"
        "Host: " + HostForMe() + "\r\n"
        "Referer: https://" + HostForMe() + "/" + BngSettings.TargetId + "\r\n"
        "User-Agent: " + Ds().Get(BNGCTX::DB::USERAGENT) + "\r\n"
        "Accept: application/json, text/javascript, */*; q=0.01\r\n"
        "Content-Type: application/x-www-form-urlencoded; charset=UTF-8\r\n"
        "Cookie: " + Ds().Get(BNGCTX::DB::COOKIE) + "\r\n"
        "X-Requested-With: XMLHttpRequest\r\n"
        "X-ab-Split-Group: " + Ds().Get(BNGCTX::DB::ABSPLITGROUP) + "\r\n"
        "Content-Length: " + std::to_string(PayloadLen) + "\r\n"
        "Connection: close\r\n"
        "\r\n"
        + Payload
    );
    Req().on_error(
        [&](CHttpReq& req, int32 errcode)
        {
            if (errcode)
                OUTPUTLN("http req failed with error %d - %s", errcode, CHttpReq::errcode_to_str(errcode));

            RequestYield();
        }
    );
    Req().on_complete(
        [&](CHttpReq& req, CHttpResponse& resp)
        {
            if (resp.status() != httpstatus::code_ok)
            {
                OUTPUTLN("http req failed with status %d", resp.status());
                RequestYield();
                return;
            };

            bool bResult = false;

            try
            {
                CJson json(resp.body(), resp.body_size());

                Ds().Set(BNGCTX::DB::DATAKEY,       json["localData"]["dataKey"].data());
                Ds().Set(BNGCTX::DB::MYNAME,        json["userData"]["username"].data());
                Ds().Set(BNGCTX::DB::MYNAMEDISP,    json["userData"]["displayName"].data());
                Ds().Set(BNGCTX::DB::ISRU,          json["userData"]["isRu"].data());
                Ds().Set(BNGCTX::DB::LOCATION,      json["userData"]["location"].data());
                Ds().Set(BNGCTX::DB::CHATHOST,      json["userData"]["chathost"].data());

                ASSERT(Ds().Get(BNGCTX::DB::DATAKEY).size());
                ASSERT(Ds().Get(BNGCTX::DB::MYNAME).size());
                ASSERT(Ds().Get(BNGCTX::DB::MYNAMEDISP).size());
                ASSERT(Ds().Get(BNGCTX::DB::ISRU).size());
                ASSERT(Ds().Get(BNGCTX::DB::LOCATION).size());

                bResult = true;
            }
            catch (CJson::exception& e)
            {
                REF(e);
                OUTPUTLN("json --> %s", e.what());
            };

            if (bResult)
                Subject().ServiceRequest(BNGCTX::SVCFLAG_SEQ_NEXT);
            else
                RequestYield();
        }
    );
    if (BngSettings.ProxyUseMode)
        Req().set_proxy(Subject().GetProxyType(), Subject().GetProxyAddr());
    if (!Req().send("https://" + HostForMe()))
        RequestYield();
};


//
//  Bng manifest state obj
//
void CBngStateChat::Attach(void* Param)
{
    m_iMsgSequenceNo = 0;
    m_eStep = STEP_JOIN;

    Ws().on_error(
        [&](CWebsocket& ws, int32 errcode)
        {
            RequestYield();
        }
    );
    Ws().on_connect(
        [&](CWebsocket& ws)
        {
            ws.set_timeout(BNGCTX::WSS_TIMEOUT);
            
            const char* PayloadFmt = "{\"id\":1,\"name\":\"joinRoom\",\"args\":[\"%s\",{\"username\":\"%s\",\"displayName\":\"%s\",\"location\":\"%s\",\"chathost\":\"%s\",\"isRu\":%s,\"isPerformer\":false,\"hasStream\":false,\"isLogged\":false,\"isPayable\":false,\"showType\":\"public\"},\"%s\"]}";
			char Payload[1024 * 2];
            Payload[0] = '\0';

            int32 PayloadLen = sprintf_s(
                Payload,
                sizeof(Payload),
                PayloadFmt,
				Ds().Get(BNGCTX::DB::CHATHOST).c_str(),     // roomname
                Ds().Get(BNGCTX::DB::MYNAME).c_str(),       // username
                Ds().Get(BNGCTX::DB::MYNAMEDISP).c_str(),   // displayName
                Ds().Get(BNGCTX::DB::LOCATION).c_str(),     // location
                Ds().Get(BNGCTX::DB::CHATHOST).c_str(),     // chathost
                Ds().Get(BNGCTX::DB::ISRU).c_str(),         // isRu
                Ds().Get(BNGCTX::DB::DATAKEY).c_str()       // token
            );

            m_eStep = STEP_JOIN;
            ws.send_txt(Payload, PayloadLen);
        }
    );
    Ws().on_read(
        [&](CWebsocket& ws, const char* data, uint32 datalen, bool binary)
        {
            switch (m_eStep)
            {
            case STEP_JOIN:
                {
					static const char ExpectedResponse[] = "{\"id\":1";
					static const char UnexpectedResponse[] = "GHOST_DETECT";
                    if (std::strstr(data, ExpectedResponse))
                    {
						static const char Payload[] = "{\"id\":2,\"name\":\"ChatModule.connect\",\"args\":[\"public-chat\"]}";
                        m_eStep = STEP_CONNECT;
                        ws.send_txt(Payload, sizeof(Payload) - 1);
                    }
                    else if (std::strstr(data, UnexpectedResponse))
                    {
                        ws.close();
                        CBngResult::Instance().SetError(CBngResult::ERRTYPE_BOT_DETECTED);
                        Subject().ServiceRequest(BNGCTX::SVCFLAG_EOL);
                        OUTPUTLN("They are detect us!!");
                    };
                }
                break;

            case STEP_CONNECT:
                {
					static const char ExpectedResponse[] = "{\"id\":2";
                    if (std::strstr(data, ExpectedResponse))
                    {
                        m_eStep = STEP_PING;
                        SendPing();
                    };
                }
                break;

            case STEP_PING:
                {
                    char ExpectedResponse[256] = "{\"id\":";
                    std::strcat(ExpectedResponse, std::to_string(m_iMsgSequenceNo).c_str());
                    if (std::strstr(data, ExpectedResponse))
                    {
                        m_iMsgSequenceNo++;
                        m_eStep = STEP_TRASH;
                        Subject().ServiceRequest(BNGCTX::SVCFLAG_YIELD | BNGCTX::SVCFLAG_SEQ_SAVE);
                    };
                }
                break;

            case STEP_TRASH:
                {
                    ;
                }
                break;

            default:
                {
                    ASSERT(false);
                    ws.abort();
                }
                break;
            };
        }
    );
    Ws().on_disconnect(
        [&](CWebsocket& ws, CWebsocket::closecode closecode)
        {
            if (closecode != CWebsocket::closecode_normal)
                OUTPUTLN("unexpected closecode %d - %s", closecode, CWebsocket::closecode_to_str(closecode));
            
            RequestYield();
        }
    );
    Ws().set_request(
        "GET " + Ds().Get(BNGCTX::DB::WSS) + "/websocket HTTP/1.1\r\n"
        "Host: " + WebUrlExtractDomain(Ds().Get(BNGCTX::DB::WSS)) + "\r\n"
        "User-Agent: " + Ds().Get(BNGCTX::DB::USERAGENT) + "\r\n"
        "Accept: */*\r\n"
        "Origin: https://" + HostForMe() + "\r\n"	// RFC 6455, page 18, 8
        "Upgrade: websocket\r\n"
        "Sec-WebSocket-Extensions: permessage-deflate; client_max_window_bits\r\n"
        "Sec-WebSocket-Key: " + CBase64::Encode(WebRndHexString(16)) + "\r\n" // RFC 6455, page 17, 7
        "Sec-WebSocket-Version: 13\r\n"
        "Connection: keep-alive, Upgrade\r\n"
        "\r\n"
    );
    Ws().set_compress(true);
    Ws().set_masking(true);
    Ws().set_timeout(BNGCTX::HTTP_TIMEOUT);
    if (BngSettings.ProxyUseMode)
        Ws().set_proxy(Subject().GetProxyType(), Subject().GetProxyAddr());
    if (!Ws().connect(Ds().Get(BNGCTX::DB::WSS)))
    {
        OUTPUTLN("ws connect failed");
        RequestYield();
    };
};


void CBngStateChat::Detach(void)
{
    ;
};


void CBngStateChat::SendPing(void)
{
    const char* PayloadFmt = "{\"id\":%d,\"name\":\"ping\"}";
    char Payload[128] = { 0 };
    Payload[0] = '\0';

    m_eStep = STEP_PING;
    int32 PayloadLen = sprintf_s(Payload, sizeof(Payload), PayloadFmt, m_iMsgSequenceNo);

    Ws().send_txt(Payload, PayloadLen);
};


//
//  Bng ctx obj
//
/*static*/ CQueue<uint64>* CBngObjCtx::m_pProxyQueue = nullptr;


CBngObjCtx::CBngObjCtx(void)
: m_StateInitPhase1()
, m_StateInitPhase2()
, m_StateChat()
, m_Shared()
, m_nStateSequenceCur(STATE_LABEL_EOL)
, m_nStateSequenceHead(STATE_LABEL_EOL)
, m_nStateSequenceTail(STATE_LABEL_EOL)
, m_nStateSequenceSave(STATE_LABEL_EOL)
, m_SleepTimer()
, m_bRequestedYield(false)
, m_bInitFlag(false)
, m_bBotFlag(false)
{
    SetShared(&m_Shared);
};


CBngObjCtx::~CBngObjCtx(void)
{
	;
};


void CBngObjCtx::Start(void)
{
    if (!m_bInitFlag)
    {
        m_bInitFlag = true;

        m_Shared.m_DataStore.Set(BNGCTX::DB::USERAGENT, UserAgentGenereate());
        m_Shared.m_RetryNo = 0;
        
        m_nStateSequenceHead = (BngSettings.ProxyUseMode ? BNGCTX::STATE_HOST_RESOLVE : BNGCTX::STATE_INIT_PHASE1);
        m_nStateSequenceTail = BNGCTX::STATE_CHATPROC;
        m_nStateSequenceCur = m_nStateSequenceHead;
        m_nStateSequenceSave = STATE_LABEL_EOL;

        if (BngSettings.ProxyUseMode)
            StateRegist(BNGCTX::STATE_HOST_RESOLVE, &m_StateHostResolve, false);
        StateRegist(BNGCTX::STATE_INIT_PHASE1, &m_StateInitPhase1, false);
        StateRegist(BNGCTX::STATE_INIT_PHASE2, &m_StateInitPhase2, false);
        StateRegist(BNGCTX::STATE_CHATPROC, &m_StateChat, false);
        
		m_SleepTimer.SetAlarm(60000 + RndUInt32(45000, 55000));
        m_SleepTimer.Reset();
    }
    else
    {
        if (m_nStateSequenceSave != STATE_LABEL_EOL)
        {
            //
            //  If seq was saved check sleep timer for ping otherwise reset sequence
            //
            m_SleepTimer.Period();
            if (!m_SleepTimer.IsAlarm())
            {
                m_bRequestedYield = true;
                return;
            };

            m_nStateSequenceCur = m_nStateSequenceSave;
            m_nStateSequenceSave = STATE_LABEL_EOL;
            m_StateChat.SendPing();
            ServiceRequest(BNGCTX::SVCFLAG_SEQ_SAVE);
            return;
        }
        else
        {            
            //
            //  If proxy mode enabled and host already resolved for proxy ip skip first state
            //
            if (BngSettings.ProxyUseMode)
                m_nStateSequenceCur = m_nStateSequenceHead + (m_Shared.m_HostForMe.empty() ? 0 : 1);
            else
                m_nStateSequenceCur = m_nStateSequenceHead;            
            m_Shared.m_RetryNo = 0;
            m_Shared.m_DataStore.Clear();
            m_Shared.m_DataStore.Set(BNGCTX::DB::USERAGENT, UserAgentGenereate());
        };
    };

    if (StrideNextProxy())
        ServiceRequest(BNGCTX::SVCFLAG_SEQ_NEXT);
    else
        ServiceRequest(BNGCTX::SVCFLAG_YIELD);    
};


void CBngObjCtx::Stop(void)
{
    m_bRequestedYield = false;
};


void CBngObjCtx::Service(uint32 uServiceFlags)
{
    if (IS_FLAG_SET(uServiceFlags, BNGCTX::SVCFLAG_EOL))
    {
        StateJump(STATE_LABEL_EOL);
        return;
    };

    if (IS_FLAG_SET(uServiceFlags, BNGCTX::SVCFLAG_RETRY))
    {
        StateJump(CurrentStateLabel());
        return;
    };

    if (IS_FLAG_SET(uServiceFlags, BNGCTX::SVCFLAG_SEQ_SAVE))
    {
        if (m_nStateSequenceSave == STATE_LABEL_EOL)
        {
            m_SleepTimer.Reset();
            m_nStateSequenceSave = CurrentStateLabel();
        };
    };

    if (IS_FLAG_SET(uServiceFlags, BNGCTX::SVCFLAG_YIELD))
    {
        //
        //  Count bot as success if yiled requested with seq save request
        //
        if (IS_FLAG_SET(uServiceFlags, BNGCTX::SVCFLAG_SEQ_SAVE))
        {
            if (!m_bBotFlag)
            {
                CBngResult::Instance().AddViewerCountWork(1);
                m_bBotFlag = true;
            };
        }
        else
        {
            if (m_bBotFlag)
            {
                CBngResult::Instance().AddViewerCountWork(-1);
                m_bBotFlag = false;
            };
        };            
        
        m_bRequestedYield = true;
        return;        
    };

    if (IS_FLAG_SET(uServiceFlags, BNGCTX::SVCFLAG_SEQ_NEXT))
    {
#ifdef _DEBUG
        if (CurrentStateLabel() == BNGCTX::STATE_INIT_PHASE2)
            OUTPUTLN("BOT NAME: %s", m_Shared.m_DataStore.Get(BNGCTX::DB::MYNAMEDISP).c_str());
#endif
        StateJump(m_nStateSequenceCur);
        m_nStateSequenceCur = Clamp(m_nStateSequenceCur + 1, m_nStateSequenceHead, m_nStateSequenceTail);
    };
};


bool CBngObjCtx::IsRequestedYield(void) const
{
    return m_bRequestedYield;
};


bool CBngObjCtx::IsRequestedEol(void) const
{
    return CProxyObj::IsEol();
};


void CBngObjCtx::SetSharedReq(CHttpReq* pReq)
{
    ASSERT(pReq);
    m_Shared.m_pReq = pReq;
};


bool CBngObjCtx::StrideNextProxy(void)
{
    bool bResult = false;

    if (BngSettings.ProxyUseMode)
    {
        ASSERT(m_pProxyQueue);

        uint64 CurPrxAddr = GetProxyAddr();
        if (NetAddrIsValid(&CurPrxAddr))
            m_pProxyQueue->Push(CurPrxAddr);

        if (!m_pProxyQueue->IsEmpty())
        {
            uint64 PrxAddr = m_pProxyQueue->Front();
            m_pProxyQueue->Pop();

            ASSERT(NetAddrIsValid(&PrxAddr));

            SetProxyAddr(PrxAddr);
			SetProxyType(NetProxy_t(BngSettings.ProxyType));

            //
            //  detect new host for new ip
            //
            m_Shared.m_HostForMe = {};
            m_Shared.m_RetryNo = 0;
            
            bResult = true;
        }
        else
        {
            uint64 EmptyPrxAddr;
            NetAddrInit(&EmptyPrxAddr);
            SetProxyAddr(EmptyPrxAddr);
        };
    }
    else
    {
        bResult = true;
    };

    return bResult;
};