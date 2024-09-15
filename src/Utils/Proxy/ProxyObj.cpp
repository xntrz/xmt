#include "ProxyObj.hpp"

#include "Shared/Network/NetAddr.hpp"


CProxyObj::CProxyObj()
: m_shared(nullptr)
, m_proxyType(NETPROXY_HTTP)
, m_proxyAddr(0)
, m_svcFlags(0)
, m_stateCur(STATE_STOP)
, m_stateReq(STATE_STOP)
, m_stateArray()
, m_bStopFlag(false)
{
    NetAddrInit(&m_proxyAddr);
};


CProxyObj::~CProxyObj()
{
    ;
};


void CProxyObj::Run()
{
    if (m_stateCur != STATE_STOP)
        m_stateArray[m_stateCur]->Observing();

    uint32 svcFlags = m_svcFlags.load();
    m_svcFlags.store(0);
    if (svcFlags != 0)
        Service(svcFlags);

    if (m_stateReq != STATE_STOP)
    {
        if (m_stateCur != STATE_STOP)
            m_stateArray[m_stateCur]->Detach();

        m_stateCur = m_stateReq.load();
        
        if (m_stateCur != STATE_STOP)
            m_stateArray[m_stateCur]->Attach();

        m_stateReq = STATE_STOP;
    };
};


void CProxyObj::StateRegist(int32 id, CState* pState)
{
    ASSERT(pState);
    ASSERT(m_stateArray[id] == nullptr);    

    pState->m_pSubject = this;
    m_stateArray[id] = pState;
};


void CProxyObj::StateJump(int32 id)
{
    if (id == STATE_STOP)
    {
        if (m_stateCur != STATE_STOP)
        {
            m_stateArray[m_stateCur]->Detach();
            m_stateCur = STATE_STOP;
        };

        m_bStopFlag = true;
    };

	m_stateReq = int16(id);
};