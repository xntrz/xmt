#include "PrxObj.hpp"
#include "PrxResult.hpp"
#include "PrxSettings.hpp"

#include "Shared/Network/Net.hpp"

#include "Utils/Misc/WebUtils.hpp"
#include "Utils/Proxy/ProxyObj.hpp"
#include "Utils/Http/HttpReq.hpp"


namespace PRX
{
    const uint32 SYSTEM_PERIOD_INTERVAL = 32;    

    enum STATE
    {
        STATE_MAIN = 0,
    };
    
    enum SVCFLAG
    {
        SVCFLAG_STRIDE = BIT(0),
        SVCFLAG_RESULT = BIT(1),
    };
};


class CPrxStateMain final : public IProxyObjState
{
public:
    virtual void Attach(void* Param) override;
    virtual void Detach(void) override {};
    virtual void Observing(void) override {};
    inline CPrxResult::RESULTTYPE GetResult(void) { return m_Result; };

private:
    CHttpReq m_req;
    int32 m_nRetryNo;
    CPrxResult::RESULTTYPE m_Result;
    std::function<void()> m_FnSendRequest;
};


class CPrxObj final : public CProxyObj
{
public:
    static std::atomic<int32> m_iProxyAddrIndex;

    virtual void Start(void) override;
    virtual void Stop(void) override;
    virtual void Service(uint32 ServiceFlags) override;

private:
    CPrxStateMain m_StateMain;
};


typedef CProxyObjSystem<CPrxObj> CPrxObjSystem;

static CPrxObjSystem* PrxObjSystem;


void CPrxStateMain::Attach(void* Param)
{
    m_nRetryNo = 0;
    m_Result = CPrxResult::RESULTTYPE_OK;

    m_FnSendRequest = [&]()
    {
        CPrxResult::Instance().NoteStart();

        if (!m_req.send(
            std::string(PrxSettings.TargetHost) + ":" + std::to_string(PrxSettings.TargetPort),
            PrxSettings.Timeout))
        {
            m_Result = CPrxResult::RESULTTYPE_FAIL;
            Subject().ServiceRequest(PRX::SVCFLAG_STRIDE | PRX::SVCFLAG_RESULT);
        };
    };

    m_req.set_timeout(PrxSettings.Timeout);
    m_req.set_proxy(Subject().GetProxyType(), Subject().GetProxyAddr());
    m_req.resolve_redirect(true);
    m_req.set_request(
        PrxSettings.FlagRequest ?
        ("GET / HTTP/1.1\r\n"
        "Host: " + std::string(PrxSettings.TargetHost) + "\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n"
        "User-Agent: " + std::string(PrxSettings.UserAgent) + "\r\n"
        "upgrade-insecure-requests: 1\r\n"
        "cache-control: no-cache\r\n"
        "sec-fetch-site: same-origin\r\n"
        "\r\n")
        :
        ("")
    );
    m_req.on_complete(
        [&](CHttpReq& req, CHttpResponse& resp)
        {
            CPrxResult::Instance().NoteStop();

            //
            //  If request flag is set branch response status otherwise just mark it as success
            //
            if (PrxSettings.FlagRequest)
                m_Result = (resp.status() == httpstatus::code_ok ? CPrxResult::RESULTTYPE_OK : CPrxResult::RESULTTYPE_FAIL);
            else
                m_Result = CPrxResult::RESULTTYPE_OK;
            
            Subject().ServiceRequest(PRX::SVCFLAG_STRIDE | PRX::SVCFLAG_RESULT);
        }
    );
    m_req.on_error(
        [&](CHttpReq& req, int32 errcode)
        {
            CPrxResult::Instance().NoteStop();
            if (m_nRetryNo++ >= PrxSettings.Retry)
            {
                m_Result = CPrxResult::RESULTTYPE_FAIL;
                Subject().ServiceRequest(PRX::SVCFLAG_STRIDE | PRX::SVCFLAG_RESULT);
            }
            else
            {
				m_FnSendRequest();
            };
        }
    );
	m_FnSendRequest();
};


/*static*/ std::atomic<int32> CPrxObj::m_iProxyAddrIndex = 0;


void CPrxObj::Start(void)
{
    StateRegist(PRX::STATE_MAIN, &m_StateMain, false);
    
    ServiceRequest(PRX::SVCFLAG_STRIDE);
};


void CPrxObj::Stop(void)
{
    ;
};


void CPrxObj::Service(uint32 ServiceFlags)
{
    if (IS_FLAG_SET(ServiceFlags, PRX::SVCFLAG_RESULT))
    {
        CPrxResult::Instance().NoteResult(m_StateMain.GetResult(), GetProxyAddr());
    };

    if (IS_FLAG_SET(ServiceFlags, PRX::SVCFLAG_STRIDE))
    {
        if (m_iProxyAddrIndex >= CPrxResult::Instance().ImportList().count())
        {
            StateJump(STATE_LABEL_EOL);
        }
        else
        {
            uint64 ProxyAddr = CPrxResult::Instance().ImportList().addr_at(m_iProxyAddrIndex++);
            ASSERT(NetAddrIsValid(&ProxyAddr));

            SetProxyAddr(ProxyAddr);
            SetProxyType(NetProxy_t(PrxSettings.Type));

            StateJump(PRX::STATE_MAIN);
        };
    };
};


void PrxObjStart(void)
{
    CPrxObj::m_iProxyAddrIndex = 0;    
    int32 nWorkpool = Min(PrxSettings.Workpool, CPrxResult::Instance().ImportList().count());

    PrxObjSystem = new CPrxObjSystem(nWorkpool, PRX::SYSTEM_PERIOD_INTERVAL);
    ASSERT(PrxObjSystem);
    if (PrxObjSystem)
        PrxObjSystem->Start();
};


void PrxObjStop(void)
{
    if (PrxObjSystem)
    {
        PrxObjSystem->Stop();
        delete PrxObjSystem;
        PrxObjSystem = nullptr;
    };
};


bool PrxObjIsEol(void)
{
	if (PrxObjSystem)
		return PrxObjSystem->IsEol();
	else
		return false;
};