#include "HttpReq.hpp"

#include "Utils/Misc/WebUtils.hpp"

#include "Shared/Common/Event.hpp"
#include "Shared/Network/Net.hpp"
#include "Shared/Network/NetAddr.hpp"


struct CHttpReq::context final : public CListNode<context>
{
	enum callback_type
	{
		callback_complete = 0,
		callback_error,
	};

	enum errcode
	{
		errcode_noerr = 0,
		errcode_transport,
		errcode_security,
		errcode_proxy,
		errcode_unreach,
		errcode_aborted_during_recv,
		errcode_canceled,
		errcode_redirect_empty,
		errcode_redirect_resend,
		errcode_send_subsys,
		errcode_send_endpointinv,
		errcode_send_inprogress,
		errcode_send_transport,
		
		errcodenum,
	};

	enum opt
	{
		opt_complete = BIT(0),
		opt_inprogress = BIT(1),
	};

	context(void);
	~context(void);
	void invoke(callback_type cbtype, int32 errcode);
	void on_req_construct(CHttpReq* req);
	void on_req_destruct(CHttpReq* req);
	bool send(const std::string& Url, uint32 ConnectTimeout);
	void resolve_redirect(bool flag);
	void opt_set(uint32 o, bool flag);
	bool opt_test(uint32 o);

	HOBJ m_hEventReady;
	HOBJ m_hConn;
	int32 m_errcode;
	CHttpReq* m_pReq;
	redirect* m_pRedirect;
	std::string m_Request;
	CHttpResponse m_Response;
	CompleteCallback m_CallbackComp;
	ErrorCallback m_CallbackErr;
	std::recursive_mutex m_CallbackMutex;
	struct
	{
		std::string domain;
		uint16 port;
	} m_endpoint;
	std::atomic<uint32> m_optmask;
};


struct CHttpReq::redirect final
{	
	bool on_complete(context& ctx, CHttpResponse& resp);
};


struct CHttpReq::netevent final
{
	static bool proc(
		HOBJ        hConn,
		NetEvent_t  Event,
		uint32      ErrorCode,
		uint64      NetAddr,
		const char* Data,
		uint32      DataSize,
		void* 		Param
	);
};


struct CHttpReq::container final
{
public:
	void init(void);
	void term(void);
	void ref_inc(void);
	void ref_dec(void);
	void regist(CHttpReq* req);
	void remove(CHttpReq* req);
	void suspend(void);
	void resume(void);
	bool is_paused(void);
	void cancelation_proc(void);
	bool is_run(void) const;

private:	
	std::atomic<int32> m_iRefCount;
	std::mutex m_Mutex;
	CList<CHttpReq> m_ListReq;
	HOBJ m_hEventRefEnd;
	bool m_bFlagRun;
	std::atomic<int32> m_iPauseLevel;
};


static CHttpReq::container HttpReqContainer;


CHttpReq::context::context(void)
: m_hEventReady(EventCreate())
, m_hConn(0)
, m_errcode(errcode_noerr)
, m_pReq(nullptr)
, m_pRedirect(nullptr)
, m_Request()
, m_Response()
, m_CallbackComp()
, m_CallbackErr()
, m_CallbackMutex()
, m_endpoint()
, m_optmask(0)
{
	opt_set(opt_complete, true);
	opt_set(opt_inprogress, false);
};


CHttpReq::context::~context(void)
{
	if (m_hConn)
	{
		NetTcpClose(m_hConn);
		m_hConn = 0;
	};

	if (m_hEventReady)
	{
		EventDestroy(m_hEventReady);
		m_hEventReady = 0;
	};

	if (m_pRedirect)
	{
		delete m_pRedirect;
		m_pRedirect = nullptr;
	};
};


void CHttpReq::context::invoke(callback_type cbtype, int32 errcode)
{	
	std::unique_lock<std::recursive_mutex> Lock(m_CallbackMutex);
	
	switch (cbtype)
	{
	case callback_complete:
		{
			opt_set(opt_inprogress, false);
			
			//
			//	By default is true to allow invoke user callback if redirect is not handling
			//
			bool bResult = true;
			
			//
			//	Intercept for handling redirect if requested
			//
			if (m_pRedirect)
				bResult = m_pRedirect->on_complete(*this, m_Response);

			if (bResult)
			{
				if (m_CallbackComp)
					m_CallbackComp(*m_pReq, m_Response);

				//
				//	check if request is not initiated in callback
				//
				if (!opt_test(opt_inprogress))
				{
					opt_set(opt_complete, true);
					EventSignalAll(m_hEventReady);
				};				
			};
		}
		break;

	case callback_error:
		{
			opt_set(opt_inprogress, false);
			
			m_errcode = errcode;
			if (m_CallbackErr)
				m_CallbackErr(*m_pReq, errcode);

			//
			//	check if request is not initiated in callback
			//
			if (!opt_test(opt_inprogress))
			{
				opt_set(opt_complete, true);
				EventSignalAll(m_hEventReady);
			};			
		}
		break;

	default:
		ASSERT(false);
		break;
	};
};


void CHttpReq::context::on_req_construct(CHttpReq* req)
{
	m_hConn = NetTcpOpen(netevent::proc, this);
	
	std::unique_lock<std::recursive_mutex> Lock(m_CallbackMutex);
	m_pReq = req;
};


void CHttpReq::context::on_req_destruct(CHttpReq* req)
{
	//
	//	Now close guaranteed that all threads leave event proc before close
	//
	if (m_hConn)
	{
		NetTcpClose(m_hConn);
		m_hConn = 0;
	};
	
	std::unique_lock<std::recursive_mutex> Lock(m_CallbackMutex);
	m_CallbackComp = {};
	m_CallbackErr = {};
	m_pReq = nullptr;
};


bool CHttpReq::context::send(const std::string& Url, uint32 ConnectTimeout)
{
	if (HttpReqContainer.is_paused())
	{
		invoke(context::callback_error, context::errcode_send_subsys);
		return false;
	};

	if (opt_test(opt_inprogress))
	{
		invoke(context::callback_error, context::errcode_send_inprogress);
		return false;
	};

	uint16 Port = 0;
	std::string Host = WebUrlExtractDomain(Url);
	std::string Proto = WebUrlExtractProto(Url);
	
	std::transform(
		Proto.begin(),
		Proto.end(),
		Proto.begin(),
		[](char ch)
		{
			return std::tolower(ch); 
		}
	);

	if (!Proto.empty())
	{
		if (Proto.compare("http") == 0)
			Port = 80;
		else if (Proto.compare("https") == 0)
			Port = 443;
	}
	else
	{
		std::string PortStr = WebUrlExtractPort(Url);
		if (!PortStr.empty())
			Port = std::atoi(PortStr.c_str());
	};
	
	m_endpoint.domain = Host;
	m_endpoint.port = Port;

	if (m_endpoint.port == 0 ||
		m_endpoint.domain.empty())
	{
		invoke(context::callback_error, context::errcode_send_endpointinv);
		return false;
	};

	switch (Port)
	{
	case 443:
		{
			NetTcpSetSecure(m_hConn, true);
			NetTcpSetSecureHost(m_hConn, Host.c_str());
		}
		break;

	case 80:
		{
			NetTcpSetSecure(m_hConn, false);
		}
		break;
	};

	m_errcode = errcode_noerr;
	m_Response.clear();
	opt_set(opt_complete, false);
	opt_set(opt_inprogress, true);
	ConnectTimeout = (ConnectTimeout > 1000 ? ConnectTimeout : 20000);

	NetTcpSetUserParam(m_hConn, (void*)ConnectTimeout);
	
	if (!NetTcpConnect(m_hConn, m_endpoint.domain.c_str(), m_endpoint.port, ConnectTimeout))
	{
		opt_set(opt_complete, true);
		opt_set(opt_inprogress, false);
		invoke(context::callback_error, context::errcode_send_transport);
		return false;
	};

	return true;
};


void CHttpReq::context::resolve_redirect(bool flag)
{
	if (flag)
	{
		if (!m_pRedirect)
		{
			m_pRedirect = new redirect;
			ASSERT(m_pRedirect);
		};
	}
	else
	{
		if (m_pRedirect)
		{
			delete m_pRedirect;
			m_pRedirect = nullptr;
		};
	};
};


void CHttpReq::context::opt_set(uint32 o, bool flag)
{
	if (flag)
		m_optmask.fetch_or(o);
	else
		m_optmask.fetch_and(~o);
};


bool CHttpReq::context::opt_test(uint32 o)
{
	return IS_FLAG_SET(m_optmask.load(), o);
};


bool CHttpReq::redirect::on_complete(context& ctx, CHttpResponse& resp)
{
	//
	//	handle "connect only" case (if request empty)
	//
	if (ctx.m_Request.empty())
		return true;

	//
	//	response is not redirect
	//
	if (!httpstatus::is_redirect(resp.status()))
		return true;

	//
	//	response not contain location header
	//
	std::string Url = resp.header_value("location");
	if (Url.empty())
	{
		ctx.invoke(context::callback_error, context::errcode_redirect_empty);
		return false;
	};

	//
	//	Replace host with redirect location
	//
	WebStrRep(ctx.m_Request, ctx.m_endpoint.domain, WebUrlExtractDomain(Url));

	//
	//	Send request again
	//
	if (!ctx.send(Url, 5000))
	{
		ctx.invoke(context::callback_error, context::errcode_redirect_resend);
		return false;
	};

	return false;
};


/*static*/ bool CHttpReq::netevent::proc(
	HOBJ        hConn,
	NetEvent_t  Event,
	uint32      ErrorCode,
	uint64      NetAddr,
	const char* Data,
	uint32      DataSize,
	void* 		Param
)
{
	if (!HttpReqContainer.is_run())
		return false;

	HttpReqContainer.ref_inc();

	ASSERT(Param);
	CHttpReq::context& Ctx = *(CHttpReq::context*)Param;

	switch (Event)
	{
	case NetEvent_ConnectFail:
	case NetEvent_ConnectFailProxy:
	case NetEvent_ConnectFailSsl:
		{
			int32 errcode = context::errcode_noerr;

			if (Event == NetEvent_ConnectFail)
				errcode = context::errcode_transport;
			else if (Event == NetEvent_ConnectFailProxy)
				errcode = context::errcode_proxy;
			else if (Event == NetEvent_ConnectFailSsl)
				errcode = context::errcode_security;
			else
				errcode = context::errcodenum;	// force unknown error
			
			Ctx.invoke(context::callback_error, errcode);
		}
		break;

	case NetEvent_Connect:
		{
			if (Ctx.m_Request.length() > 0)
				NetTcpSend(Ctx.m_hConn, &Ctx.m_Request[0], Ctx.m_Request.length());
			else
				NetTcpDisconnect(Ctx.m_hConn);
		}
		break;

	case NetEvent_Recv:
		{
			CHttpResponse& Response = Ctx.m_Response;

			if (!Response.process(Data, DataSize))
			{
				NetTcpDisconnect(Ctx.m_hConn);
				break;
			};

			if (!Response.is_complete())
				break;

			NetTcpDisconnect(Ctx.m_hConn);
		}
		break;

	case NetEvent_Disconnect:
		{						
			if (!Ctx.m_Request.empty())
			{
				if (!Ctx.m_Response.is_complete())
				{
					//
					//	May be disconnected by server bandwidth limitation per one IP during many reads
					// 	So interpret it as transport error
					//
					Ctx.invoke(context::callback_error, context::errcode_aborted_during_recv);
				}
				else
				{
					Ctx.invoke(context::callback_complete, context::errcode_noerr);
				};
			}
			else
			{
				Ctx.invoke(context::callback_complete, context::errcode_noerr);
			};
		}
		break;
	};

	HttpReqContainer.ref_dec();

	return true;
};


void CHttpReq::container::init(void)
{
	m_iRefCount = 0;
	m_ListReq.clear();
	m_hEventRefEnd = EventCreate();
	m_bFlagRun = true;
};


void CHttpReq::container::term(void)
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


void CHttpReq::container::ref_inc(void)
{
	++m_iRefCount;
};


void CHttpReq::container::ref_dec(void)
{
	ASSERT(m_iRefCount > 0);
	if (!--m_iRefCount)
	{
		if (!is_run())
			EventSignalAll(m_hEventRefEnd);
	};
};


void CHttpReq::container::regist(CHttpReq* req)
{
	std::unique_lock<std::mutex> Lock(m_Mutex);
	m_ListReq.push_back(req);
};


void CHttpReq::container::remove(CHttpReq* req)
{
	std::unique_lock<std::mutex> Lock(m_Mutex);
	m_ListReq.erase(req);
};


void CHttpReq::container::suspend(void)
{
	++m_iPauseLevel;
	cancelation_proc();
};


void CHttpReq::container::resume(void)
{
	ASSERT(m_iPauseLevel > 0);
	--m_iPauseLevel;
};


bool CHttpReq::container::is_paused(void)
{
	return (m_iPauseLevel > 0);
};


void CHttpReq::container::cancelation_proc(void)
{
	std::unique_lock<std::mutex> Lock(m_Mutex);
	for (auto& it : m_ListReq)
		it.cancel();
};


bool CHttpReq::container::is_run(void) const
{
	return m_bFlagRun;
};


/*static*/ void CHttpReq::initialize(void)
{
	HttpReqContainer.init();
};


/*static*/ void CHttpReq::terminate(void)
{
	HttpReqContainer.term();
};


/*static*/ void CHttpReq::suspend(void)
{
	HttpReqContainer.suspend();
};


/*static*/ void CHttpReq::resume(void)
{
	HttpReqContainer.resume();
};


/*static*/ void CHttpReq::cancel_all(void)
{
	HttpReqContainer.cancelation_proc();
};


/*static*/ const char* CHttpReq::errcode_to_str(int32 errcode)
{
	static const char* const ErrcodeToStrTbl[] =
	{
		"No error",
		"Transport error",
		"tls/ssl error",
		"proxy error",
		"host is unreach",
		"connection was aborted while doing recv",
		"request was canceled",
		"redirect location is empty",
		"redirect resend request error",
		"send failed subsystem down",
		"send failed endpoint invalid",
		"send failed already in progress",
		"send failed transport layer internal error",
	};

	static_assert(COUNT_OF(ErrcodeToStrTbl) == context::errcodenum, "update me");

	if (errcode >= 0 && errcode < COUNT_OF(ErrcodeToStrTbl))
		return ErrcodeToStrTbl[errcode];
	else
		return "Unknown error";
};


CHttpReq::CHttpReq(void)
: m_pContext(nullptr)
{	
	m_pContext = new context;
	m_pContext->on_req_construct(this);
	
	HttpReqContainer.regist(this);
};


CHttpReq::~CHttpReq(void)
{
	HttpReqContainer.remove(this);
	
	if (m_pContext)
	{
		m_pContext->on_req_destruct(this);
		delete m_pContext;
		m_pContext = nullptr;
	};
};


void CHttpReq::cancel(void)
{
	NetTcpCancelConnect(ctx().m_hConn);
	NetTcpDisconnect(ctx().m_hConn);	
};


bool CHttpReq::send(const std::string& Url, uint32 ConnectTimeout)
{
	return ctx().send(Url, ConnectTimeout);
};


void CHttpReq::on_complete(CompleteCallback cb)
{
	if (!ctx().opt_test(context::opt_complete))
		return;

	ctx().m_CallbackComp = cb;
};


void CHttpReq::on_error(ErrorCallback cb)
{
	if (!ctx().opt_test(context::opt_complete))
		return;

	ctx().m_CallbackErr = cb;
};


void CHttpReq::set_request(const std::string& Request)
{
	if (!ctx().opt_test(context::opt_complete))
		return;

	ctx().m_Request = Request;
};


void CHttpReq::set_timeout(uint32 ms)
{
	NetTcpSetTimeoutRead(ctx().m_hConn, ms);
};


void CHttpReq::set_proxy(NetProxy_t ProxyType, uint64 NetAddr, void* Parameter, int32 ParameterLen)
{
	NetTcpClearProxy(ctx().m_hConn);
	NetTcpSetProxy(ctx().m_hConn, ProxyType, NetAddr, Parameter, ParameterLen);
};


bool CHttpReq::is_complete(void)
{
	return ctx().opt_test(context::opt_complete);
};


CHttpResponse& CHttpReq::response(void)
{
	return ctx().m_Response;
};


void CHttpReq::wait(void)
{
	if (ctx().opt_test(context::opt_complete))
		return;

	EventWait(
		ctx().m_hEventReady,
		[](void* Param) { return ((CHttpReq::context*)Param)->opt_test(context::opt_complete); },
		&ctx()
	);
};


bool CHttpReq::wait(uint32 ms)
{
	if (ctx().opt_test(context::opt_complete))
		return true;

	return EventWaitFor(
		ctx().m_hEventReady,
		ms,
		[](void* Param) { return ((CHttpReq::context*)Param)->opt_test(context::opt_complete); },
		&ctx()
	);
};


void CHttpReq::resolve_redirect(bool flag)
{
	if (!ctx().opt_test(context::opt_complete))
		return;

	ctx().resolve_redirect(flag);
};


std::string CHttpReq::get_host(void)
{
	return ctx().m_endpoint.domain;
};


int32 CHttpReq::get_errcode(void)
{
	return ctx().m_errcode;
};


CHttpReq::context& CHttpReq::ctx(void)
{
	ASSERT(m_pContext);
	return *m_pContext;
};