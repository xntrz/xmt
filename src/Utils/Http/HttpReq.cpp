#include "HttpReq.hpp"

#include "Utils/Misc/WebUtils.hpp"
#include "Utils/module_obj.hpp"

#include "Shared/Common/Event.hpp"
#include "Shared/Common/Spinlock.hpp"
#include "Shared/Network/Net.hpp"
#include "Shared/Network/NetAddr.hpp"
#ifdef _DEBUG
#include "Shared/File/File.hpp"
#endif


/**
 * 	Thread local flag for error callback check is error occurs durin sync user call or async network call
 *	If flag is up all error codes has MSB is set up (see CHttpReq::errcode_is_user() & CHttpReq::context::invoke())
 */
thread_local bool net_thread_flag = false;


struct CHttpReq::context final
{
	enum callback_type
	{
		callback_complete = 0,
		callback_error,
	};
	
	enum errcode : uint32
	{
		errcode_noerr = 0,
		errcode_transport,
		errcode_security,
		errcode_proxy,
		errcode_aborted_during_recv,
		errcode_already_connected,
		errcode_redirect_depth_reached,
		errcode_endpoint_invalid,
		
		errcodenum,
	};

	enum flag : uint16
	{
		flag_redirect 	= (1 << 0),
		flag_keepalive 	= (1 << 1),
#ifdef _DEBUG
		flag_dump 		= (1 << 12),
#endif		
		flag_default 	= 0,
	};

	context(CHttpReq& req);
	~context();
	void close();
	void cancel();
	bool invoke(callback_type cbtype, uint32 errcode = errcode_noerr);
	bool send_first(const std::string& url, const std::string& request, std::chrono::milliseconds timeout);
	bool send_next(const std::string& request);
	void flag_set(uint16 f, bool state);
	bool flag_test(uint16 f);
	void set_read_timeout(std::chrono::milliseconds ms);
	void set_proxy(NETPROXY type, uint64 netaddr, const NETPROXYPARAM* param);
	void clear_proxy();
	void wait();
	bool wait(std::chrono::milliseconds ms);
	bool event_proc(HOBJ hConn, NETEVENT evt, uint32 errcode, uint64 netaddr, const void* data, uint32 dize, void* param);
	bool redirect_proc();
	void dump_on_complete(bool state);

	inline bool is_complete() const {
		return !NetTcpIsConnected(m_hConn);
	};
	
	inline void ref_inc() {
		++m_refCnt;
	};
	
	inline void ref_dec() {
		ASSERT(m_refCnt > 0u);
		if (!--m_refCnt)
		{
			ASSERT(m_refCntUser == 0u);
			delete this;
		};
	};

	inline void ref_inc_user() {
		if (!m_refCntUser++)
			ref_inc();
	};

	inline void ref_dec_user() {
		ASSERT(m_refCntUser > 0u);
		if (!--m_refCntUser)
		{
			close();
			ref_dec();
		}
		else
		{
			ASSERT(false);
		};
	};

	CEvent 						m_evtReady;
	HCONN 						m_hConn;
	uint32 						m_errcode;
	CHttpReq& 					m_reqObj;
	std::string 				m_reqBody;
	CHttpResponse 				m_response;
	CompleteCallback 			m_cbComplete;
	ErrorCallback 				m_cbError;
	std::recursive_mutex 		m_cbMutex;
	struct
	{
		std::string domain;
		uint16 port;
	} m_endpoint;
	std::atomic<uint16> 		m_flags;
	std::atomic<int8> 			m_redirectMax;
	std::atomic<int8> 			m_redirectCur;
	std::atomic<uint16> 		m_refCnt;
	std::atomic<uint16> 		m_refCntUser;
	std::chrono::milliseconds	m_connectionTimeout;
	module_obj 					m_moduleObj;
};


CHttpReq::context::context(CHttpReq& req)
: m_evtReady()
, m_hConn(NetTcpOpen(std::bind(&context::event_proc, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7)))
, m_errcode(errcode_noerr)
, m_reqObj(req)
, m_reqBody()
, m_response()
, m_cbComplete(nullptr)
, m_cbError(nullptr)
, m_cbMutex()
, m_endpoint()
, m_flags(flag_default)
, m_redirectMax(0u)
, m_redirectCur(0u)
, m_refCnt(0u)
, m_refCntUser(0u)
, m_connectionTimeout(0)
, m_moduleObj("http")
{
	ASSERT(m_hConn);
	module_obj_regist(&m_moduleObj);
};


CHttpReq::context::~context()
{
	module_obj_remove(&m_moduleObj);
	
	if (m_hConn) 
	{
		NetTcpClose(m_hConn);
		m_hConn = 0;
	};
};


void CHttpReq::context::close()
{
	cancel();
	
	std::unique_lock<std::recursive_mutex> lock(m_cbMutex);
	m_cbComplete = nullptr;
	m_cbError = nullptr;
};


void CHttpReq::context::cancel()
{
	NetTcpCancelConnect(m_hConn);
	NetTcpDisconnect(m_hConn);
};


bool CHttpReq::context::invoke(callback_type cbtype, uint32 errcode /*= errcode_noerr*/)
{	
	std::unique_lock<std::recursive_mutex> lock(m_cbMutex);

	bool bResult = true;

	switch (cbtype) 
	{
	case callback_complete:
		{
			/* intercept for handling redirect if requested */
			if (flag_test(flag_redirect) && httpstatus::is_redirect(m_response.status()))
			{
				if (m_redirectCur++ < m_redirectMax) 
				{
					bResult = redirect_proc();
				}
				else 
				{
					invoke(callback_error, errcode_redirect_depth_reached);
					bResult = false;
				};
			};

			if (bResult)
				bResult = (m_cbComplete ? m_cbComplete(m_reqObj, m_response) : false);			
		}
		break;

	case callback_error:
		{
			m_errcode = errcode;
			if (m_cbError)
				m_cbError(m_reqObj, (net_thread_flag ? errcode : (errcode | 0x80000000u)));
		}
		break;

	default:
		ASSERT(false);
		break;
	};

	return bResult;
};


bool CHttpReq::context::send_first(const std::string& url, const std::string& request, std::chrono::milliseconds timeout)
{
	if (NetTcpIsConnected(m_hConn))
	{
		invoke(context::callback_error, context::errcode_already_connected);
		return false;
	};

	/* extract domain */
	m_endpoint.domain.clear();
	m_endpoint.domain = WebUrlExtractDomain(url);
	if (m_endpoint.domain.empty())
	{
		invoke(context::callback_error, context::errcode_endpoint_invalid);
		return false;
	};

	/* extract port */
	m_endpoint.port = 0;
	std::string proto = WebUrlExtractProto(url);
	if (!proto.empty())
	{
		/* proto is exist in url - match proto to port number */
		strtolower(proto);
		if (proto == "http" || proto == "ws")
			m_endpoint.port = 80;
		else if (proto == "https" || proto == "wss")
			m_endpoint.port = 443;
		else
			DbgFatal("unknown protocol port for http request: %s", proto.c_str());
	}
	else
	{
		/* proto not exist in url - to extract port number */
		std::string port = WebUrlExtractPort(url);
		if (!port.empty())
			m_endpoint.port = std::stoi(port);
		else
			m_endpoint.port = 80; /* proto & port number do not exist in url, set HTTP 80 port as default case */
	};

	if (m_endpoint.port == 0)
	{
		invoke(context::callback_error, context::errcode_endpoint_invalid);
		return false;
	};

	/* setup per connect data */
	m_errcode = errcode_noerr;
	m_response.clear();
	m_connectionTimeout = std::max(timeout, std::chrono::milliseconds(1000));
	m_reqBody = request;
	flag_set(flag_keepalive, false);

	/* setup connection ssl */
	NetTcpSetSecure(m_hConn, (WebIsSecurePort(m_endpoint.port) ? true : false));
	if (WebIsSecurePort(m_endpoint.port))
		NetTcpSetSecureHost(m_hConn, m_endpoint.domain.c_str());

	/* start connect */
	ref_inc();
	if (!NetTcpConnect(m_hConn, m_endpoint.domain.c_str(), m_endpoint.port, uint32(m_connectionTimeout.count())))
	{
		invoke(context::callback_error, context::errcode_transport);
		ref_dec();
		return false;
	};

	return true;
};


bool CHttpReq::context::send_next(const std::string& request)
{
	if (request.empty())
		return false;

	if (!flag_test(flag_keepalive))
		return false;

	m_response.clear();
	return (NetTcpSend(m_hConn, &request[0], request.length()) > 0);
};


void CHttpReq::context::flag_set(uint16 f, bool state)
{
	(state ? m_flags.fetch_or(f) : m_flags.fetch_and(~f));
};


bool CHttpReq::context::flag_test(uint16 f)
{
	return ((m_flags.load() & f) == f);
};


void CHttpReq::context::set_read_timeout(std::chrono::milliseconds ms)
{
	NetTcpSetTimeoutRead(m_hConn, uint32(ms.count()));
};


void CHttpReq::context::set_proxy(NETPROXY type, uint64 netaddr, const NETPROXYPARAM* param)
{
	NetTcpClearProxy(m_hConn);
	NetTcpSetProxy(m_hConn, type, netaddr, param);
};


void CHttpReq::context::clear_proxy()
{
	NetTcpClearProxy(m_hConn);
};


void CHttpReq::context::wait()
{
	return m_evtReady.wait([this]() {
		return !NetTcpIsConnected(m_hConn);
	});
};


bool CHttpReq::context::wait(std::chrono::milliseconds ms)
{
	return m_evtReady.wait_for(ms, [this]() {
		return !NetTcpIsConnected(m_hConn);
	});
};


bool CHttpReq::context::event_proc(HOBJ hConn, NETEVENT evt, uint32 errcode, uint64 netaddr, const void* data, uint32 size, void* param)
{
	bool bResult = true;
	net_thread_flag = true;

	switch (evt) {
	case NETEVENT_CONNECTFAIL:
	case NETEVENT_CONNECTFAILPRX:
	case NETEVENT_CONNECTFAILSSL:
		{
			uint32 err = context::errcode_transport;
			if (evt == NETEVENT_CONNECTFAILPRX)
				err = context::errcode_proxy;
			else if (evt == NETEVENT_CONNECTFAILSSL)
				err = context::errcode_security;

			invoke(context::callback_error, err);

			m_evtReady.signal_all();
		}
		break;

	case NETEVENT_CONNECT:
		{
			ref_inc();

			if (m_reqBody.length() > 0)
				NetTcpSend(m_hConn, &m_reqBody[0], m_reqBody.length());
			else
				bResult = false;
		}
		break;

	case NETEVENT_RECV:
		{
			ref_inc();

			if (!m_response.process(data, size))
			{
				bResult = false;
				break;
			};

			if (!m_response.is_complete())
				break;

#ifdef _DEBUG
			/**
			 *	DEBUG ONLY
			 *	Make dump of html page when request is complete
			 */
			if (flag_test(flag_dump))
			{				
				HOBJ hFile = 0;
				int32 no = 0;
				std::string filename;

				/**
				 *	Check file name for busy
				 *	So if we able to open file for reading that mean file is exist
				 *	Iterate until we find not existing file for new one
				 */
				do
				{
					filename = "http_req_dump_" + std::to_string(no++) + ".html";
					hFile = FileOpen(filename.c_str(), "rb");
				} while (hFile);

				hFile = FileOpen(filename.c_str(), "wb");
				if (hFile)
				{
					FileWrite(hFile, m_response.body(), uint32(m_response.body_size()));
					FileFlush(hFile);
					FileClose(hFile);
				};
			};
#endif

			flag_set(flag_keepalive, m_response.is_keepalive());
			if (flag_test(flag_keepalive))
			{
				if (!invoke(context::callback_complete))
					bResult = false;
				else
					m_response.clear();
			}
			else
			{
				bResult = false;
			};
		}
		break;

	case NETEVENT_DISCONNECT:
		{
			if (!m_reqBody.empty() && !m_response.is_complete() && !flag_test(flag_keepalive))
			{
				/* May be disconnected by server bandwidth limitation per one IP during many reads or invalid response */
				invoke(context::callback_error, context::errcode_aborted_during_recv);
			}
			else
			{
				invoke(context::callback_complete);
			};

			/* wait will not end until connection is opened (in user callback or redirect callback for example) */
			m_evtReady.signal_all();
		}
		break;
	};

	ref_dec();

	net_thread_flag = false;
	return bResult;
};


bool CHttpReq::context::redirect_proc()
{
	/**
	 *	RETURN VALUE MEANING:
	 *		FALSE	-	delay completion callback until we got right location
	 *		TRUE	-	we got final location allow processing completion callback
	 */

	/* handle "connect only" case (if request empty) */
	if (m_reqBody.empty())
		return true;

	/* response not contain location header */
	std::string Url = m_response.header_value("location");
	if (Url.empty())
		return false;

	/* Replace host with redirect location */
	std::string loc = WebUrlExtractDomain(Url);
	ASSERT(loc.empty() == false);
	httputils::change_request_location(m_reqBody, loc);

	/* initiate request again with new location */
	if (!send_first(Url, m_reqBody, m_connectionTimeout))
		return false;

	return false;
};


void CHttpReq::context::dump_on_complete(bool state)
{
#ifdef _DEBUG
	flag_set(flag_dump, state);
#endif
};


/*static*/ const char* CHttpReq::errcode_to_str(uint32 errcode)
{
	static const char* const errcode2str[] = {
		"No error",
		"Transport error",
		"TLS/SSL error",
		"Proxy error",
		"Connection was aborted while doing recv",
		"Already connected",
		"Redirect depth reached",
		"Endpoint invalid",
	};

	static_assert(COUNT_OF(errcode2str) == context::errcodenum, "update me");
	
	errcode &= 0x7FFFFFFFu; // check code value

	return (errcode < COUNT_OF(errcode2str) ? errcode2str[errcode] : "unknown error");
};


/*static*/ bool CHttpReq::errcode_is_user(uint32 errcode)
{
	return ((errcode & 0x80000000u) == 0x80000000u); // check flag value
};


CHttpReq::CHttpReq()
: m_pContext(nullptr)
{	
	m_pContext = new context(*this);
	m_pContext->ref_inc_user();
};


CHttpReq::CHttpReq(CHttpReq&& r)
{
	m_pContext = r.m_pContext;
	r.m_pContext = nullptr;
};


CHttpReq::CHttpReq(const CHttpReq& r)
{
	m_pContext = r.m_pContext;
	m_pContext->ref_inc_user();
};


CHttpReq& CHttpReq::operator=(const CHttpReq& r)
{
	m_pContext = r.m_pContext;
	m_pContext->ref_inc_user();
	return *this;
};


CHttpReq::~CHttpReq()
{
	if (m_pContext)
		m_pContext->ref_dec_user();
};


void CHttpReq::close()
{
	ctx().close();
};


void CHttpReq::cancel()
{
	ctx().cancel();
};


void CHttpReq::send(const std::string& url, const std::string& request, std::chrono::milliseconds timeout /*= std::chrono::milliseconds(0)*/)
{
	ctx().m_redirectCur = 0u;
	ctx().send_first(url, request, timeout);
};


void CHttpReq::send(const std::string& request)
{
	ctx().m_redirectCur = 0u;
	ctx().send_next(request);
};


void CHttpReq::on_complete(CompleteCallback cb)
{
	//if (ctx().flag_test(context::flag_ready))
		ctx().m_cbComplete = cb;
};


void CHttpReq::on_error(ErrorCallback cb)
{
	//if (ctx().flag_test(context::flag_ready))
		ctx().m_cbError = cb;
};


void CHttpReq::set_read_timeout(std::chrono::milliseconds ms)
{
	ctx().set_read_timeout(ms);
};


void CHttpReq::set_proxy(NETPROXY type, uint64 netaddr, const NETPROXYPARAM* param /*= nullptr*/)
{
	ctx().set_proxy(type, netaddr, param);
};


void CHttpReq::clear_proxy()
{
	ctx().clear_proxy();
};


void CHttpReq::resolve_redirect(bool state, int32 depth /*= 7*/)
{
	ctx().m_redirectMax = int8(state ? depth : 0);
	ctx().flag_set(context::flag_redirect, state);
};


CHttpResponse& CHttpReq::response()
{
	return ctx().m_response;
};


bool CHttpReq::is_complete()
{
	return ctx().is_complete();
};


HCONN CHttpReq::conn_handle()
{
	return ctx().m_hConn;
};


std::string CHttpReq::host()
{
	return ctx().m_endpoint.domain;
};


uint32 CHttpReq::errcode()
{
	return ctx().m_errcode;
};


void CHttpReq::wait()
{
	ctx().wait();
};


bool CHttpReq::wait(std::chrono::milliseconds ms)
{
	return ctx().wait(ms);
};


void CHttpReq::dump_on_complete(bool state)
{
#ifdef _DEBUG
	ctx().dump_on_complete(state);
#endif	
};


CHttpReq::context& CHttpReq::ctx()
{
	ASSERT(m_pContext);
	return *m_pContext;
};