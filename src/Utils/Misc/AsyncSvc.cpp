#include "AsyncSvc.hpp"

#include "Shared/Common/AsyncService.hpp"


CAsyncSvc::CAsyncSvc(const std::string& Label, uint32 UpdPeriod)
: m_hAsyncSvcObj(0)
{
    AsyncSvcCallbacks_t Callbacks = { 0 };
    Callbacks.Start = [](void* Param) { ((CAsyncSvc*)Param)->OnStart(); };
    Callbacks.Stop = [](void* Param) { ((CAsyncSvc*)Param)->OnStop(); };
    Callbacks.Run = [](void* Param) { ((CAsyncSvc*)Param)->OnRun(); };

    m_hAsyncSvcObj = AsyncSvcCreate(Label.c_str(), &Callbacks, UpdPeriod, this);
    ASSERT(m_hAsyncSvcObj != 0);
};


CAsyncSvc::~CAsyncSvc(void)
{
    ASSERT(!AsyncSvcIsRunning(m_hAsyncSvcObj));
    
    if (m_hAsyncSvcObj)
    {
        AsyncSvcDestroy(m_hAsyncSvcObj);
        m_hAsyncSvcObj = 0;
    };
};


void CAsyncSvc::Start(void)
{
    if (!AsyncSvcIsRunning(m_hAsyncSvcObj))
        AsyncSvcStart(m_hAsyncSvcObj);
};


void CAsyncSvc::Stop(void)
{
    if (AsyncSvcIsRunning(m_hAsyncSvcObj))
        AsyncSvcStop(m_hAsyncSvcObj);
};