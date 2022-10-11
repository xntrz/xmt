#include "Websocket.hpp"
#include "WebsocketStream.hpp"

#include "Utils/Misc/WebUtils.hpp"
#include "Utils/Http/HttpResponse.hpp"

#include "Shared/Common/Event.hpp"
#include "Shared/Network/Net.hpp"
#include "Shared/Network/NetAddr.hpp"


struct CWebsocket::context final : public CListNode<context>
{
    enum callback_type
    {
        callback_conn = 0,
        callback_dc,
        callback_rd_bin,
        callback_rd_txt,
        callback_err,
        callback_pong,
    };

    enum state
    {
        state_idle = 0,
        state_connect,
        state_upgrade,
        state_run,
    };

    enum errcode
    {
        errcode_noerr = 0,
        errcode_not_reach,
        errcode_upgrade_failed,
        errcode_in_use,
        errcode_stream,

        errcodenum,
    };

    enum opt
    {
        opt_masking = BIT(0),
        opt_compress = BIT(1),
        opt_closesent = BIT(2),
        opt_closed = BIT(3),
    };

    context(void);
    ~context(void);
    void invoke(callback_type cbtype, const char* Data, uint32 DataLen, int32 Errcode);
    void set_state(state s);
    void set_opt(uint32 o, bool flag);
    bool test_opt(uint32 o);
    void clear_opt_per_session(void);
    bool is_idle(void);
    bool is_run(void);
    void on_ws_construct(CWebsocket* ws);
    void on_ws_destruct(CWebsocket* ws);
    bool check_terms_for_connect(void);
    void on_shutdown(void);
    
    CWebsocket* m_pWs;
    CWebsocketStream m_wss;
    std::recursive_mutex m_WriteMutex;
    CHttpResponse m_response;
    HOBJ m_hEventReady;
    HOBJ m_hConn;
    std::string m_Request;
    ConnectCallback m_CallbackConn;
    DisconnectCallback m_CallbackDc;
    ReadCallback m_CallbackRd;
    ErrorCallback m_CallbackErr;
    PongCallback m_CallbackPong;
    std::mutex m_CallbackMutex;
    struct
    {
        std::string domain;
        uint16 port;
    } m_endpoint;
    std::atomic<state> m_eState;
    std::atomic<uint32> m_optmask;
    closecode m_Closecode;
};


struct CWebsocket::netevent final
{
    static bool proc(
        HOBJ        hConn,
        NetEvent_t  Event,
        uint32      ErrorCode,
        uint64      NetAddr,
        const char* Data,
        uint32      DataSize,
        void*       Param
    );
};


struct CWebsocket::container final
{
public:
    void init(void);
    void term(void);
    void ref_inc(void);
    void ref_dec(void);
    void regist(CWebsocket* ws);
    void remove(CWebsocket* ws);
    void abort_all(void);
    bool is_run(void) const;

private:
    std::atomic<int32> m_iRefCount;
    std::mutex m_Mutex;
    CList<CWebsocket> m_ListWs;
    HOBJ m_hEventRefEnd;
    bool m_bFlagRun;
};


static CWebsocket::container WebsocketContainer;


CWebsocket::context::context(void)
: m_pWs(nullptr)
, m_wss()
, m_WriteMutex()
, m_response()
, m_hEventReady(EventCreate())
, m_hConn(0)
, m_Request()
, m_CallbackConn()
, m_CallbackDc()
, m_CallbackRd()
, m_CallbackErr()
, m_CallbackMutex()
, m_endpoint({})
, m_eState(state_idle)
, m_optmask(0)
, m_Closecode(closecode_normal)
{
    ;
};


CWebsocket::context::~context(void)
{
    if (m_hEventReady)
    {
        EventDestroy(m_hEventReady);
        m_hEventReady = 0;
    };

    if (m_hConn)
    {
        NetTcpClose(m_hConn);
        m_hConn = 0;
    };
};


void CWebsocket::context::invoke(callback_type cbtype, const char* Data, uint32 DataLen, int32 Errcode)
{
    std::unique_lock<std::mutex> Lock(m_CallbackMutex);

    switch (cbtype)
    {
    case callback_conn:
        if (m_CallbackConn)
            m_CallbackConn(*m_pWs);
        break;

    case callback_dc:
        if (m_CallbackDc)
            m_CallbackDc(*m_pWs, closecode(Errcode));
        break;

    case callback_rd_bin:
        if (m_CallbackRd)
            m_CallbackRd(*m_pWs, Data, DataLen, true);
        break;

    case callback_rd_txt:
        if (m_CallbackRd)
            m_CallbackRd(*m_pWs, Data, DataLen, false);
        break;

    case callback_err:
        if (m_CallbackErr)
            m_CallbackErr(*m_pWs, Errcode);
        break;

    case callback_pong:
        if (m_CallbackPong)
            m_CallbackPong(*m_pWs, Data, DataLen);
        break;

    default:
        ASSERT(false);
        break;
    };
};


void CWebsocket::context::set_state(state s)
{
    m_eState = s;
};


void CWebsocket::context::set_opt(uint32 o, bool flag)
{
    if (flag)
        m_optmask.fetch_or(o);
    else
        m_optmask.fetch_and(~o);
};


bool CWebsocket::context::test_opt(uint32 o)
{
    return IS_FLAG_SET(m_optmask.load(), o);
};


void CWebsocket::context::clear_opt_per_session(void)
{
    set_opt(opt_closed, false);
    set_opt(opt_closesent, false);
};


bool CWebsocket::context::is_idle(void)
{
    return (m_eState.load() == state_idle);
};


bool CWebsocket::context::is_run(void)
{
    return (m_eState.load() == state_run);
};


void CWebsocket::context::on_ws_construct(CWebsocket* ws)
{
    m_hConn = NetTcpOpen(netevent::proc, this);
    
    std::unique_lock<std::mutex> Lock(m_CallbackMutex);
    m_pWs = ws;
};


void CWebsocket::context::on_ws_destruct(CWebsocket* ws)
{
    if (m_hConn)
    {
        NetTcpClose(m_hConn);
        m_hConn = 0;
    };
    
    std::unique_lock<std::mutex> Lock(m_CallbackMutex);
    m_CallbackConn = {};
    m_CallbackDc = {};
    m_CallbackRd = {};
    m_CallbackErr = {};
    m_pWs = nullptr;
};


bool CWebsocket::context::check_terms_for_connect(void)
{
    if (!is_idle())
        return false;

    if (m_Request.empty())
        return false;

    if (!m_CallbackConn)
        return false;

    if (!m_CallbackErr)
        return false;

    return true;
};


void CWebsocket::context::on_shutdown(void)
{
    if (m_hConn)
    {
        NetTcpSetForceClose(m_hConn, true);
        NetTcpClose(m_hConn);
        m_hConn = 0;
    };

	m_wss.end();
    m_eState = state_idle;
    m_hConn = NetTcpOpen(netevent::proc, this);
};


/*static*/ bool CWebsocket::netevent::proc(
    HOBJ        hConn,
    NetEvent_t  Event,
    uint32      ErrorCode,
    uint64      NetAddr,
    const char* Data,
    uint32      DataSize,
    void*       Param
)
{
    if (!WebsocketContainer.is_run())
        return false;

    WebsocketContainer.ref_inc();
    
    ASSERT(Param);
    CWebsocket::context& Ctx = *(CWebsocket::context*)Param;

    switch (Event)
    {
    case NetEvent_ConnectFail:
    case NetEvent_ConnectFailProxy:
    case NetEvent_ConnectFailSsl:
        {
            Ctx.clear_opt_per_session();
            Ctx.m_eState = context::state_idle;
            
            Ctx.invoke(context::callback_err, nullptr, 0, context::errcode_not_reach);
            
            //
            //  Check if connetion is not initiated in callback
            //
            if (Ctx.is_idle())
                EventSignalAll(Ctx.m_hEventReady);
        }
        break;

    case NetEvent_Connect:
    case NetEvent_Recv:
        {
            switch (Ctx.m_eState)
            {
            case context::state_connect:
                {
                    ASSERT(!Ctx.m_Request.empty());
                    Ctx.m_eState = context::state_upgrade;
                    if (!NetTcpSend(Ctx.m_hConn, &Ctx.m_Request[0], Ctx.m_Request.length()))
                        NetTcpDisconnect(Ctx.m_hConn);
                }
                break;

            case context::state_upgrade:
                {
                    CHttpResponse& Response = Ctx.m_response;

                    if (!Response.process(Data, DataSize))
                    {
                        NetTcpDisconnect(Ctx.m_hConn);
                        break;
                    };

                    if (!Response.is_complete())
                        break;

                    httpstatus::value code = Response.status();
                    Response.clear();

                    if (code == httpstatus::code_switching_protocol)
                    {
                        uint32 WsFeatures = 0;                        
                        int32 WsBits = CWebsocketStream::BITS_DEFAULT;
                        
                        if (Ctx.test_opt(context::opt_compress))
                            WsFeatures |= CWebsocketStream::FEATURE_COMPRESS;
                        
                        if (Ctx.test_opt(context::opt_masking))
                            WsFeatures |= CWebsocketStream::FEATURE_MASKING;

                        if (Ctx.m_wss.begin(WsBits, WsFeatures))
                        {
                            Ctx.m_eState = context::state_run;
                            Ctx.invoke(context::callback_conn, nullptr, 0, 0);
                        }
                        else
                        {
                            NetTcpDisconnect(Ctx.m_hConn);
                        };
                    }
                    else
                    {
                        NetTcpDisconnect(Ctx.m_hConn);
                    };
                }
                break;

            case context::state_run:
                {
                    //
                    //  Websocket stream use two different stream for READ and WRITE
                    //  Only one thread per recv call will access READ stream
                    //
                    CWebsocketStream& WsStream = Ctx.m_wss;

                    int32 ret = WsStream.read(Data, DataSize);
                    if (ret >= CWebsocketStream::RESULT_OK)
                    {
                        CWebsocketStream::MESSAGE Msg = { 0 };
                        while (WsStream.read_message(Msg))
                        {
                            //
                            //  FIXME handle OPCODE_CLOSE
                            //
                            if (Ctx.test_opt(context::opt_closesent) ||
                                Ctx.test_opt(context::opt_closed))
                            {
								if ((Msg.m_opcode == CWebsocketStream::OPCODE_BINARY) ||
									(Msg.m_opcode == CWebsocketStream::OPCODE_TEXT) ||
									(Msg.m_opcode == CWebsocketStream::OPCODE_PONG))
								{
									continue;
								};
                            };

                            const char* data = nullptr;
                            uint32 datalen = 0;

                            if (Msg.m_data.size())
                            {
                                data = (const char*)&Msg.m_data[0];
                                datalen = Msg.m_data.size();
                            };                            

                            if (Msg.m_opcode == CWebsocketStream::OPCODE_BINARY)
                            {
                                Ctx.invoke(context::callback_rd_bin, data, datalen, 0);
                            }
                            else if (Msg.m_opcode == CWebsocketStream::OPCODE_TEXT)
                            {
                                Ctx.invoke(context::callback_rd_txt, data, datalen, 0);
                            }
                            else if (Msg.m_opcode == CWebsocketStream::OPCODE_PING)
                            {
                                //
                                //  TODO ws ping opcode
                                //
                            }
                            else if (Msg.m_opcode == CWebsocketStream::OPCODE_PONG)
                            {
                                Ctx.invoke(context::callback_pong, data, datalen, 0);
                            }
                            else if (Msg.m_opcode == CWebsocketStream::OPCODE_CLOSE)
                            {
                                Ctx.set_opt(context::opt_closed, true);
                                Ctx.m_Closecode = closecode(SWAP2(*((closecode*)&data[0])));
                                NetTcpDisconnect(Ctx.m_hConn);
                                break;
                            }
                            else
                            {
                                OUTPUTLN("got unexpected opcode or continue %d", Msg.m_opcode);
                                NetTcpDisconnect(Ctx.m_hConn);
                            };
                        };
                    }
                    else
                    {
                        NetTcpDisconnect(Ctx.m_hConn);
                    };
                }
                break;

            default:
                ASSERT(false);
                break;
            };
        }
        break;

    case NetEvent_Disconnect:
        {
            context::state eState = Ctx.m_eState;

            Ctx.m_wss.end();
            Ctx.m_eState = context::state_idle;

            if (eState == context::state_run)
                Ctx.invoke(context::callback_dc, nullptr, 0, Ctx.m_Closecode);
            else
                Ctx.invoke(context::callback_err, nullptr, 0, context::errcode_upgrade_failed);

            //
            //  Check if connetion is not initiated in callback
            //
            if (Ctx.is_idle())
                EventSignalAll(Ctx.m_hEventReady);
        }
        break;
    };

    WebsocketContainer.ref_dec();

    return true;
};


void CWebsocket::container::init(void)
{
    m_iRefCount = 0;
    m_ListWs.clear();
    m_hEventRefEnd = EventCreate();
    m_bFlagRun = true;
};


void CWebsocket::container::term(void)
{
    m_bFlagRun = false;

    if (m_iRefCount > 0)
    {
        if (!EventWaitFor(m_hEventRefEnd, 5000))
            ASSERT(false);
    };

    if (m_hEventRefEnd)
    {
        EventDestroy(m_hEventRefEnd);
        m_hEventRefEnd = 0;
    };
};


void CWebsocket::container::ref_inc(void)
{
    ++m_iRefCount;
};


void CWebsocket::container::ref_dec(void)
{
    ASSERT(m_iRefCount > 0);
    if (!--m_iRefCount)
    {
        if (!is_run())
            EventSignalAll(m_hEventRefEnd);
    };    
};


void CWebsocket::container::regist(CWebsocket* ws)
{
    std::unique_lock<std::mutex> Lock(m_Mutex);
    m_ListWs.push_back(ws);
};


void CWebsocket::container::remove(CWebsocket* ws)
{
    std::unique_lock<std::mutex> Lock(m_Mutex);
    m_ListWs.erase(ws);
};


void CWebsocket::container::abort_all(void)
{
    std::unique_lock<std::mutex> Lock(m_Mutex);
    for (auto& it : m_ListWs)
        it.abort();
};


bool CWebsocket::container::is_run(void) const
{
    return m_bFlagRun;
};


/*static*/ void CWebsocket::initialize(void)
{
    WebsocketContainer.init();
};


/*static*/ void CWebsocket::terminate(void)
{
    WebsocketContainer.term();
};


/*static*/ void CWebsocket::abort_all(bool await_clear)
{
    WebsocketContainer.abort_all();
};


/*static*/ const char* CWebsocket::errcode_to_str(int32 code)
{
    static const char* const ErrcodeToStrTbl[] =
    {
        "No error",
        "Host is not reachable",
        "Http proto upgrade failed",
        "Connection already in use",
        "Internal stream error",
    };

    static_assert(COUNT_OF(ErrcodeToStrTbl) == context::errcodenum, "update me");

    if (code >= 0 && code < COUNT_OF(ErrcodeToStrTbl))
        return ErrcodeToStrTbl[code];
    else
        return "Unknown error";
};


/*static*/ const char* CWebsocket::closecode_to_str(closecode code)
{
    static const char* const ClosecodeToStrTbl[] =
    {
        "Normal closure",
        "Going away",
        "Protocol error",
        "Unsupported data",
        "Reserved",
        "No status received",
        "Abnormal closure",
        "Invalid frame payload data",
        "Policy violation",
        "Message too big",
        "Mandatory extension missing",
        "Internal server error",
    };

    static_assert(COUNT_OF(ClosecodeToStrTbl) == (closecodenum - 1000), "update me");

    int32 Index = (code - 1000);

    if (Index >= 0 && Index < COUNT_OF(ClosecodeToStrTbl))
        return ClosecodeToStrTbl[Index];
    else
        return "Unknown error";
};


CWebsocket::CWebsocket(void)
: m_pContext(nullptr)
{
    m_pContext = new context();
    m_pContext->on_ws_construct(this);

    WebsocketContainer.regist(this);
};


CWebsocket::~CWebsocket(void)
{
    WebsocketContainer.remove(this);
    
    if (m_pContext)
    {
        m_pContext->on_ws_destruct(this);
        delete m_pContext;
        m_pContext = nullptr;
    };
};


void CWebsocket::shutdown(void)
{
    ctx().on_shutdown();
};


void CWebsocket::abort(void)
{
    NetTcpCancelConnect(ctx().m_hConn);
    NetTcpDisconnect(ctx().m_hConn);
};


bool CWebsocket::connect(const std::string& Url, uint32 ConnectTimeout)
{
    if (!WebsocketContainer.is_run())
        return false;

    if (!ctx().check_terms_for_connect())
        return false;
    
    ctx().m_endpoint.domain = WebUrlExtractDomain(Url);
    
    std::string Proto = WebUrlExtractProto(Url);
    std::transform(Proto.begin(), Proto.end(), Proto.begin(), [](char ch) { return std::tolower(ch); });

    if (Proto.compare("ws") == 0)
    {
        ctx().m_endpoint.port = 80;
        NetTcpSetSecure(ctx().m_hConn, false);
    }
    else if (Proto.compare("wss") == 0)
    {
        ctx().m_endpoint.port = 443;
        NetTcpSetSecure(ctx().m_hConn, true);
        NetTcpSetSecureHost(ctx().m_hConn, ctx().m_endpoint.domain.c_str());
    }
    else
    {
        ctx().m_endpoint.port = 0;
        NetTcpSetSecure(ctx().m_hConn, false);
    };

    if ((ctx().m_endpoint.port == 0) ||
        (ctx().m_endpoint.domain.empty()))
        return false;

    ctx().m_eState = context::state_connect;
    ctx().m_response.clear();
    ctx().clear_opt_per_session();
    ConnectTimeout = (ConnectTimeout >= 1000 ? ConnectTimeout : 20000);

    if (!NetTcpConnect(ctx().m_hConn, ctx().m_endpoint.domain.c_str(), ctx().m_endpoint.port, ConnectTimeout))
    {
        ctx().m_eState = context::state_idle;
        return false;
    };

    return true;
};


void CWebsocket::send_bin(const char* Data, uint32 DataSize)
{
    if (!ctx().is_run())
        return;
    
    if (!send_common(Data, DataSize, CWebsocketStream::OPCODE_BINARY))
        abort();
};


void CWebsocket::send_txt(const char* Data, uint32 DataSize)
{
    if (!ctx().is_run())
        return;
    
    if (!send_common(Data, DataSize, CWebsocketStream::OPCODE_TEXT))
        abort();
};


void CWebsocket::ping(const char* Data, uint32 DataSize)
{
    if (!ctx().is_run())
        return;
    
    DataSize = Min(DataSize, 125u);
    if (!send_common(Data, DataSize, CWebsocketStream::OPCODE_PING))
        abort();
};


void CWebsocket::close(closecode code, const char* Data, uint32 DataSize)
{
    if (!ctx().is_run())
        return;
    
    if (ctx().is_run() && (!ctx().test_opt(context::opt_closesent)))
    {        
        char Payload[125]; // https://www.rfc-editor.org/rfc/rfc6455#section-5.5
        DataSize = Min(DataSize, sizeof(Payload) - sizeof(closecode));

		*((uint16*)&Payload[0]) = SWAP2(code);
        std::memcpy(&Payload[2], Data, DataSize);

        ctx().set_opt(context::opt_closesent, true);        
        if (!send_common(Payload, DataSize + sizeof(closecode), CWebsocketStream::OPCODE_CLOSE))
        {
            ctx().set_opt(context::opt_closesent, false);
            abort();
        };
    }
    else
    {
        abort();
    };
};


void CWebsocket::on_connect(ConnectCallback cb)
{
    if (!ctx().is_idle())
        return;

    ctx().m_CallbackConn = cb;
};


void CWebsocket::on_disconnect(DisconnectCallback cb)
{
    if (!ctx().is_idle())
        return;

    ctx().m_CallbackDc = cb;
};


void CWebsocket::on_read(ReadCallback cb)
{
    if (!ctx().is_idle())
        return;

    ctx().m_CallbackRd = cb;    
};


void CWebsocket::on_error(ErrorCallback cb)
{
    if (!ctx().is_idle())
        return;

    ctx().m_CallbackErr = cb;
};


void CWebsocket::on_pong(PongCallback cb)
{
    if (!ctx().is_idle())
        return;

    ctx().m_CallbackPong = cb;
};


void CWebsocket::set_timeout(uint32 ms)
{
    NetTcpSetTimeoutRead(ctx().m_hConn, ms);
};


void CWebsocket::set_request(const std::string& req)
{
    if (!ctx().is_idle())
        return;

    ctx().m_Request = req;
};


void CWebsocket::set_masking(bool flag)
{
    if (!ctx().is_idle())
        return;

    ctx().set_opt(context::opt_masking, flag);
};


void CWebsocket::set_compress(bool flag)
{
    if (!ctx().is_idle())
        return;

    ctx().set_opt(context::opt_compress, flag);
};


void CWebsocket::set_proxy(NetProxy_t ProxyType, uint64 NetAddr, void* Parameter, int32 ParameterLen)
{
    if (!ctx().is_idle())
        return;

    NetTcpClearProxy(ctx().m_hConn);
    NetTcpSetProxy(ctx().m_hConn, ProxyType, NetAddr, Parameter, ParameterLen);
};


void CWebsocket::wait(void)
{
    if (ctx().is_idle())
        return;
    
    EventWait(
        ctx().m_hEventReady,
        [](void* Param){ return ((CWebsocket::context*)Param)->is_idle(); },
        &ctx()
    );
};


bool CWebsocket::wait(uint32 ms)
{
    if (ctx().is_idle())
        return true;
    
    return EventWaitFor(
        ctx().m_hEventReady,
        ms,
        [](void* Param){ return ((CWebsocket::context*)Param)->is_idle(); },
        &ctx()
    );
};


bool CWebsocket::send_common(const char* Data, uint32 DataSize, int32 Opcode)
{
    std::unique_lock<std::recursive_mutex> Lock(ctx().m_WriteMutex);
    
    if (ctx().test_opt(context::opt_closed))
        return false;

    if (!ctx().is_run())
        return false;

    {
        std::unique_lock<std::recursive_mutex> Lock(ctx().m_WriteMutex);

        std::vector<unsigned char> Payload;
        int32 ret = ctx().m_wss.write(Data, DataSize, Payload, Opcode);
        if (ret == CWebsocketStream::RESULT_OK)
        {
            if (NetTcpSend(ctx().m_hConn, (const char*)&Payload[0], Payload.size()))
                return true;
        };
    }

    return false;
};


CWebsocket::context& CWebsocket::ctx(void)
{
    ASSERT(m_pContext);
    return *m_pContext;
};