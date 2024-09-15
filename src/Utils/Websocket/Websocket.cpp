#include "Websocket.hpp"
#include "WebsocketStream.hpp"

#include "Utils/module_obj.hpp"
#include "Utils/Misc/WebUtils.hpp"
#include "Utils/Http/HttpReq.hpp"
#include "Utils/Base64/Base64.hpp"

#include "Shared/Common/Event.hpp"
#include "Shared/Network/Net.hpp"
#include "Shared/Network/NetAddr.hpp"
#include "Shared/Common/UserAgent.hpp"


struct CWebsocket::context final
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
        state_http,
        state_ws,
    };

    enum errcode
    {
        errcode_noerr = 0,
        errcode_upgrade_failed,

        errcodenum,
    };

    enum flag : uint16
    {
        flag_masking    = (1 << 0),
        flag_compress   = (1 << 1),

        flag_default    = 0,
    };

    context(CWebsocket* ws);
    ~context();
    void close();
    void abort();
    void cancel();
    void send_first(const std::string& url, const std::string& reqBody, const std::string& ext, std::chrono::milliseconds timeout);
    bool send_next(const void* data, std::size_t size, int32 opcode);
    void send_close(const void* data, std::size_t size, int32 closecode);
    void invoke(callback_type cbtype, const void* data, std::size_t size, uint32 errcode);
    void flag_set(uint16 f, bool state);
    bool flag_test(uint16 f) const;
    void set_timeout(std::chrono::milliseconds ms);
    void set_proxy(NETPROXY type, uint64 addr, const NETPROXYPARAM* param);
    void clear_proxy();
    void wait();
    bool wait(std::chrono::milliseconds ms);
    bool event_proc(HOBJ hConn, NETEVENT evt, uint32 errcode, uint64 netaddr, const void* data, uint32 size, void* param);
    void make_default_rq(const std::string& url, const std::string& ext, std::string& reqBody) const;
    void read_extensions(const CHttpResponse& resp, int32& bits, bool& compress, bool& masking) const;

    inline bool is_idle() const {
        return (m_state == state_idle);
    };

    inline void ref_inc() {
        ++m_refCnt;
    };

    inline void ref_dec() {
		if (!--m_refCnt) {
			ASSERT(m_refCntUser == 0);
            delete this;
		};
    };

    inline void ref_inc_user() {
        if (!m_refCntUser++)
            ref_inc();
    };

    inline void ref_dec_user() {
        if (!--m_refCntUser) {
            close();
            ref_dec();
        };
    };

    CWebsocket*             m_ws;
    CWebsocketStream        m_wss;
    std::recursive_mutex    m_wssMutex;
    HCONN                   m_hConn;
    ConnectCallback         m_cbConnect;
    DisconnectCallback      m_cbDisc;
    ReadCallback            m_cbRead;
    ErrorCallback           m_cbError;
    PongCallback            m_cbPong;
    std::recursive_mutex    m_cbMutex;
    std::atomic<state>      m_state;
    std::atomic<uint16>     m_flags;
    std::atomic<int16>      m_closeSentFlag;
    closecode               m_closecode;
    std::atomic<int16>      m_refCnt;
    std::atomic<int16>      m_refCntUser;
    NETEVENTPROC            m_reqObjNetEventProc;
    void*                   m_reqObjNetEventProcParam;
    CHttpReq                m_req;
    CEvent                  m_evtReady;
    module_obj              m_moduleObj;
};


CWebsocket::context::context(CWebsocket* ws)
: m_ws(ws)
, m_wss()
, m_wssMutex()
, m_hConn(0)
, m_cbConnect(nullptr)
, m_cbDisc(nullptr)
, m_cbRead(nullptr)
, m_cbError(nullptr)
, m_cbPong(nullptr)
, m_cbMutex()
, m_state(state_idle)
, m_flags(flag_default)
, m_closecode(closecode_normal)
, m_refCnt(0)
, m_refCntUser(0)
, m_reqObjNetEventProc(nullptr)
, m_reqObjNetEventProcParam(nullptr)
, m_req()
, m_evtReady()
, m_moduleObj("ws")
{
    module_obj_regist(&m_moduleObj);
    
    /* hook http req net event proc */
    m_hConn = m_req.conn_handle();
    m_hConn = NetTcpCopy(m_hConn);
    NetTcpGetEventProc(m_hConn, &m_reqObjNetEventProc, &m_reqObjNetEventProcParam);
    NetTcpSetEventProc(m_hConn, std::bind(&context::event_proc, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7));
};


CWebsocket::context::~context()
{
    /* restore http net event proc */
    if (m_hConn)
    {
        NetTcpSetEventProc(m_hConn, m_reqObjNetEventProc, m_reqObjNetEventProcParam);
        NetTcpClose(m_hConn);
        m_hConn = 0;
    };
    
    module_obj_remove(&m_moduleObj);
};


void CWebsocket::context::close()
{
    abort();

    std::unique_lock<std::recursive_mutex> lock(m_cbMutex);
    m_ws = nullptr;
    m_cbConnect = nullptr;
    m_cbDisc = nullptr;
    m_cbRead = nullptr;
    m_cbError = nullptr;
    m_cbPong = nullptr;
};


void CWebsocket::context::abort()
{
    NetTcpSetForceClose(m_hConn, true);
    cancel();
};


void CWebsocket::context::cancel()
{
    NetTcpCancelConnect(m_hConn);
    NetTcpDisconnect(m_hConn);
};


void CWebsocket::context::send_first(
    const std::string&          url,
    const std::string&          reqBody,
    const std::string&          ext,
    std::chrono::milliseconds   timeout
)
{
    if (m_state != state_idle)
    {
        OUTPUTLN("attempt connect on already connected websocket");
		return;
	};

    /* init per connect data */
    ref_inc();
    NetTcpSetForceClose(m_hConn, false);
    m_state = state_http;

    /* init http req callbacks */
    m_req.on_complete([=](CHttpReq& req, CHttpResponse& resp) {
        if (resp.status() == httpstatus::code_upgrade_required ||
            resp.status() == httpstatus::code_switching_protocol)
        {
            /* extract websocket stream features and correct it depending on server response */
            int32 bits = -1;
            bool bCompress = false;
            bool bMasking = true;
            read_extensions(resp, bits, bCompress, bMasking);

            if (bits == -1)
                bits = CWebsocketStream::BITS_DEFAULT;

            flag_set(flag_masking, bMasking);
            flag_set(flag_compress, bCompress);

            /* setup websocket stream features */
            uint32 WsFeatures = 0;
            if (flag_test(context::flag_compress))
                WsFeatures |= CWebsocketStream::FEATURE_COMPRESS;
            if (flag_test(context::flag_masking))
                WsFeatures |= CWebsocketStream::FEATURE_MASKING;

            /* start websocket stream */
            bool wsResult = false;
            {
                std::unique_lock<std::recursive_mutex> lock(m_wssMutex);
                wsResult = m_wss.begin(CWebsocketStream::BITS_DEFAULT, WsFeatures);
            }

            /**
             * invoke connection callback if stream start successfully
             * otherwise invoke error callback and abort keepalive http request
             */
            if (wsResult)
            {
                m_state = state_ws;
                invoke(context::callback_conn, nullptr, 0, 0);
                return true;
            };
        }
        else if (req.is_complete())
        {
            return false;
		};
        m_state = state_idle;
        invoke(context::callback_err, nullptr, 0, context::errcode_upgrade_failed);
        return false;
    });
    m_req.on_error([=](CHttpReq& req, int32 errcode) {
        m_state = state_idle;
        invoke(context::callback_err, nullptr, 0, context::errcode_upgrade_failed);
        /* if error happens during initiate connection otherwise net thread will dec ref count for us */
        if (CHttpReq::errcode_is_user(errcode))
			ref_dec();
    });

    /* initiate http req */
    if (!reqBody.empty())
    {
        m_req.send(url, reqBody, timeout);
    }
    else
    {
        flag_set(context::flag_compress, true);
		flag_set(context::flag_masking, true);
        std::string defaultReqBody;
		make_default_rq(url, ext, defaultReqBody);
        m_req.send(url, defaultReqBody, timeout);
    };
};


bool CWebsocket::context::send_next(const void* data, std::size_t size, int32 opcode)
{
    std::unique_lock<std::recursive_mutex> lock(m_wssMutex);

    std::vector<unsigned char> payload;
    int32 result = m_wss.write(data, size, payload, opcode);
	if (result == CWebsocketStream::RESULT_OK)
		return (NetTcpSend(m_hConn, payload.data(), payload.size()) > 0);
    else
        ASSERT(false, "wss stream write failed (%d)\n", result);
    
    return false;
};


void CWebsocket::context::send_close(const void* data, std::size_t size, int32 closecode)
{
    char payload[125]; // https://www.rfc-editor.org/rfc/rfc6455#section-5.5
    size = std::min(size, sizeof(payload) - sizeof(closecode));

	*((uint16*)&payload[0]) = byteswap2(closecode);
	if (data && size)
		std::memcpy(&payload[2], data, size);

    send_next(payload, size + sizeof(closecode), CWebsocketStream::OPCODE_CLOSE);
};


void CWebsocket::context::invoke(callback_type cbtype, const void* data, std::size_t size, uint32 errcode)
{
    std::unique_lock<std::recursive_mutex> lock(m_cbMutex);

    switch (cbtype)
    {
    case callback_conn:
        if (m_cbConnect)
            m_cbConnect(*m_ws);
        break;

    case callback_dc:
        if (m_cbDisc)
            m_cbDisc(*m_ws, closecode(errcode));
        break;

    case callback_rd_bin:
    case callback_rd_txt:
        if (m_cbRead)
            m_cbRead(*m_ws, data, size, (cbtype == callback_rd_bin ? true : false));
        break;

    case callback_err:
        if (m_cbError)
            m_cbError(*m_ws, errcode);
        break;

    case callback_pong:
        if (m_cbPong)
            m_cbPong(*m_ws, data, size);
        break;

    default:
        ASSERT(false);
        break;
    };
};


void CWebsocket::context::flag_set(uint16 f, bool state)
{
    (state ? m_flags.fetch_or(f) : m_flags.fetch_and(~f));    
};


bool CWebsocket::context::flag_test(uint16 f) const
{
    return ((m_flags.load() & f) == f);
};


void CWebsocket::context::set_timeout(std::chrono::milliseconds ms)
{
    NetTcpSetTimeoutRead(m_hConn, uint32(ms.count()));
};


void CWebsocket::context::set_proxy(NETPROXY type, uint64 addr, const NETPROXYPARAM* param)
{
    NetTcpClearProxy(m_hConn);
    NetTcpSetProxy(m_hConn, type, addr, param);
};


void CWebsocket::context::clear_proxy()
{
    NetTcpClearProxy(m_hConn);
};


void CWebsocket::context::wait()
{
    m_evtReady.wait([this]() {
        return !NetTcpIsConnected(m_hConn);
    });
};


bool CWebsocket::context::wait(std::chrono::milliseconds ms)
{
    return m_evtReady.wait_for(ms, [this]() {
        return !NetTcpIsConnected(m_hConn);
    });
};


bool CWebsocket::context::event_proc(
    HOBJ        hConn,
    NETEVENT    evt,
    uint32      errcode,
    uint64      netaddr,
    const void* data,
    uint32      size,
    void*       param
)
{
    bool bResult = true;
    
    switch (m_state)
    {
    case context::state_http:
        {
            if ((evt == NETEVENT_CONNECT) || (evt == NETEVENT_RECV))
                ref_inc();
            
            bResult = m_reqObjNetEventProc(hConn, evt, errcode, netaddr, data, size, m_reqObjNetEventProcParam);
        }
        break;

    case context::state_ws:
        {
            switch (evt)
            {
            case NETEVENT_RECV:
                {
                    ref_inc();

                    std::vector<CWebsocketStream::message> msgArray;
                    msgArray.reserve(16);

                    int32 wsResult = -1;
                    bool breakFlag = false;

                    {
                        std::unique_lock<std::recursive_mutex> lock(m_wssMutex);
                        wsResult = m_wss.read(data, size);
                        if (wsResult >= CWebsocketStream::RESULT_OK) {
                            CWebsocketStream::message msg;
                            while (m_wss.read_message(msg))
                                msgArray.push_back(std::move(msg));
                        };
                    }

                    for (const auto& msg : msgArray)
                    {
                        if (breakFlag)
                            break;

                        const char* data = nullptr;
                        std::size_t datalen = 0;

                        if (msg.data.size())
                        {
                            data = reinterpret_cast<const char*>(msg.data.data());
                            datalen = msg.data.size();
                        };

                        switch (msg.opcode)
                        {
                        case CWebsocketStream::OPCODE_BINARY:
                            invoke(context::callback_rd_bin, data, datalen, 0);
                            break;

                        case CWebsocketStream::OPCODE_TEXT:
                            invoke(context::callback_rd_txt, data, datalen, 0);
                            break;

                        case CWebsocketStream::OPCODE_PING:
                            send_next(data, datalen, CWebsocketStream::OPCODE_PONG); // RFC 6455, 5.5.2
                            break;

                        case CWebsocketStream::OPCODE_PONG:
                            invoke(context::callback_pong, data, datalen, 0);
                            break;

                        case CWebsocketStream::OPCODE_CLOSE:
                            breakFlag = true;
                            m_closecode = closecode(byteswap2(*((closecode*)&data[0])));
                            NetTcpDisconnect(hConn);
                            break;

                        default:
                            OUTPUTLN("got unexpected opcode or continue %" PRIi32, msg.opcode);
                            NetTcpDisconnect(hConn);
                            break;
                        };
                    };
                }
                break;

            case NETEVENT_DISCONNECT:
                {
                    {
                        std::unique_lock<std::recursive_mutex> lock(m_wssMutex);
                        m_wss.end();
                    }

                    m_state = state_idle;
                    invoke(context::callback_dc, nullptr, 0, m_closecode);
                    m_evtReady.signal_all();

                    /* this call will end async ref count that we get when switching from http to ws state for recv call */
                    bResult = m_reqObjNetEventProc(hConn, evt, errcode, netaddr, data, size, m_reqObjNetEventProcParam);
                }
                break;
            };
        }
        break;
    };

    ref_dec();

    return bResult;
};


void CWebsocket::context::make_default_rq(const std::string& url, const std::string& ext, std::string& reqBody) const
{
    std::string host = WebUrlExtractDomain(url);
    std::string extensions;

    if (ext.empty())
    {
        bool bCompress = (flag_test(flag_compress) ? true : false);
        bool bNoMasking = (!flag_test(flag_masking) ? true : false);
        extensions += (bCompress || bNoMasking) ? "Sec-WebSocket-Extensions: " : "";
        extensions += (bCompress) ? "permessage-deflate; client_max_window_bits" : "";
        extensions += (bNoMasking) ? "; no-masking" : "";
        extensions += (bCompress || bNoMasking) ? "\r\n" : "";
    }
    else
    {
        extensions += "Sec-WebSocket-Extensions: ";
        extensions += ext;
        extensions += "\r\n";
    };
    
    reqBody =
        "GET " + url + " HTTP/1.1\r\n"
        "Host: " + host + "\r\n"
        "User-Agent: " + std::string(UserAgentGenereate()) + "\r\n"
        "Accept: */*\r\n"
		"Origin: " + host + "\r\n"
        "Upgrade: websocket\r\n"
        + extensions +
        "Sec-WebSocket-Key: " + CBase64::Encode(WebRndHexString(16)) + "\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Connection: keep-alive, Upgrade\r\n"
        "\r\n";
};


void CWebsocket::context::read_extensions(const CHttpResponse& resp, int32& bits, bool& compress, bool& masking) const
{    
    std::vector<std::string> values = resp.header_values("sec-websocket-extensions");
    for (const auto& value : values)
    {
        std::vector<std::pair<std::string, std::string>> hdrs = httputils::split_headers(value);

        auto search_hdr = [&hdrs](const std::string& name) {
            return std::find_if(hdrs.begin(), hdrs.end(), [&](const auto& hdr) {
                return (hdr.first == name);
            });
        };
        
        using hdr_result = std::vector<std::pair<std::string, std::string>>::iterator;

        hdr_result resultBits = search_hdr("client_max_window_bits");
        hdr_result resultCompress = search_hdr("permessage-deflate");
        hdr_result resultNoMasking = search_hdr("no-masking");

        if (resultBits != hdrs.end())
            bits = (resultBits->second.empty() ? -1 : std::stoi(resultBits->second));
        else
            bits = -1;

        compress = bool(resultCompress != hdrs.end());
        masking = !bool(resultNoMasking != hdrs.end());
    };
};


/*static*/ const char* CWebsocket::errcode_to_str(uint32 code)
{
    static const char* const errcode2str[] =
    {
        "No error",
        "Http protocol upgrade failed",        
    };

    static_assert(COUNT_OF(errcode2str) == context::errcodenum, "update me");
    ASSERT(code < COUNT_OF(errcode2str));
    
    return (code < COUNT_OF(errcode2str) ? errcode2str[code] : "unknown error");
};


/*static*/ const char* CWebsocket::closecode_to_str(closecode code)
{
    static const char* const closecode2str[] = {
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

    code = closecode(code - 1000);

    static_assert(COUNT_OF(closecode2str) == (closecodenum - 1000), "update me");
    ASSERT(code < COUNT_OF(closecode2str));

    return (code < COUNT_OF(closecode2str) ? closecode2str[code] : "unknown error");
};


CWebsocket::CWebsocket()
: m_pContext(nullptr)
{
    m_pContext = new context(this);
    m_pContext->ref_inc_user();
};


CWebsocket::CWebsocket(CWebsocket&& r)
{
    m_pContext = r.m_pContext;
    r.m_pContext = nullptr;
};


CWebsocket::CWebsocket(const CWebsocket& r)
{
    m_pContext = r.m_pContext;
    m_pContext->ref_inc_user();
};


CWebsocket& CWebsocket::operator=(const CWebsocket& r)
{
    m_pContext = r.m_pContext;
    m_pContext->ref_inc_user();
    return *this;
};


CWebsocket::~CWebsocket()
{
    if (m_pContext)
        m_pContext->ref_dec_user();
};


void CWebsocket::connect(
    const std::string&          url,
    std::chrono::milliseconds   timeout /*= std::chrono::milliseconds(0)*/
)
{
    ctx().send_first(url, {}, {}, timeout);
};


void CWebsocket::connect(
    const std::string&          url,
    const std::string&          request,
    std::chrono::milliseconds   timeout /*= std::chrono::milliseconds(0)*/
)
{
    ctx().send_first(url, request, {}, timeout);
};


void CWebsocket::connect_ex(
    const std::string&          url,
    const std::string&          wsExtensions,
    std::chrono::milliseconds   timeout /*= std::chrono::milliseconds(0)*/
)
{
    ctx().send_first(url, {}, wsExtensions, timeout);
};


void CWebsocket::send_bin(const void* data, std::size_t size)
{
    ctx().send_next(data, size, CWebsocketStream::OPCODE_BINARY);    
};


void CWebsocket::send_txt(const void* data, std::size_t size)
{
    ctx().send_next(data, size, CWebsocketStream::OPCODE_TEXT);
};


void CWebsocket::ping(const void* data /*= nullptr*/, std::size_t size /*= 0*/)
{
    const std::size_t controlFrameMaxPayloadLen = 125u;
    size = std::min(size, controlFrameMaxPayloadLen);
    ctx().send_next(data, size, CWebsocketStream::OPCODE_PING);
};


void CWebsocket::close(closecode code /*= closecodenormal*/, const void* data /*= nullptr*/, std::size_t size /*= 0*/)
{
    ctx().send_close(data, size, code);
};


void CWebsocket::abort()
{
    ctx().cancel();
};


void CWebsocket::on_connect(ConnectCallback cb)
{
    if (ctx().is_idle())
        ctx().m_cbConnect = cb;
};


void CWebsocket::on_disconnect(DisconnectCallback cb)
{
    if (ctx().is_idle())
        ctx().m_cbDisc = cb;
};


void CWebsocket::on_read(ReadCallback cb)
{
    if (ctx().is_idle())
        ctx().m_cbRead = cb;    
};


void CWebsocket::on_error(ErrorCallback cb)
{
    if (ctx().is_idle())
        ctx().m_cbError = cb;
};


void CWebsocket::on_pong(PongCallback cb)
{
    if (ctx().is_idle())
        ctx().m_cbPong = cb;
};


void CWebsocket::set_timeout(std::chrono::milliseconds ms)
{
    ctx().set_timeout(ms);
};


void CWebsocket::set_masking(bool state)
{
    if (ctx().is_idle())
        ctx().flag_set(context::flag_masking, state);
};


void CWebsocket::set_compress(bool state)
{
    if (ctx().is_idle())
        ctx().flag_set(context::flag_compress, state);
};


void CWebsocket::set_proxy(NETPROXY type, uint64 netaddr, const NETPROXYPARAM* param /*= nullptr*/)
{
    ctx().set_proxy(type, netaddr, param);
};


void CWebsocket::clear_proxy()
{
    ctx().clear_proxy();
};


void  CWebsocket::resolve_redirect(bool state, int32 depth /*= 7*/)
{
    ctx().m_req.resolve_redirect(state, depth);
};


void CWebsocket::wait()
{
    ctx().wait();
};


bool CWebsocket::wait(std::chrono::milliseconds ms)
{
    return ctx().wait(ms);
};


CWebsocket::context& CWebsocket::ctx()
{
    ASSERT(m_pContext);
    return *m_pContext;
};