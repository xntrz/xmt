#include "ProxyObj.hpp"

#include "Shared/Network/NetAddr.hpp"


CProxyObj::CProxyObj(void)
: m_SharedData(nullptr)
, m_ProxyType(NetProxy_Http)
, m_ProxyAddr(0)
, m_uServiceFlags(0)
, m_iStateLabelCurrent(STATE_LABEL_EOL)
, m_iStateLabelRequest(STATE_LABEL_EOL)
, m_StateParamRequest(0)
, m_bFlagEol(false)
{
	NetAddrInit(&m_ProxyAddr);

    for (int32 i = 0; i < COUNT_OF(m_aStatePtr); ++i)
        m_aStatePtr[i].Value = nullptr;
};


CProxyObj::~CProxyObj(void)
{
    for (int32 i = 0; i < COUNT_OF(m_aStatePtr); ++i)
    {
        if (m_aStatePtr[i].Parts.FlagAlloc)
        {
            ASSERT(m_aStatePtr[i].Value);
            delete m_aStatePtr[i].Value;
            m_aStatePtr[i].Value = nullptr;
        };
    };
};


void CProxyObj::Run(void)
{
    if (m_iStateLabelCurrent != STATE_LABEL_EOL)
        m_aStatePtr[m_iStateLabelCurrent]->Observing();

    uint32 uServiceFlags = m_uServiceFlags.load();
    m_uServiceFlags.store(0);

    if(uServiceFlags != 0)
        Service(uServiceFlags);

    if (m_iStateLabelRequest != STATE_LABEL_EOL)
    {        
        if (m_iStateLabelCurrent != STATE_LABEL_EOL)
            m_aStatePtr[m_iStateLabelCurrent]->Detach();

        m_iStateLabelCurrent = m_iStateLabelRequest.load();

        if (m_iStateLabelCurrent != STATE_LABEL_EOL)
            m_aStatePtr[m_iStateLabelCurrent]->Attach(m_StateParamRequest);

        m_iStateLabelRequest = STATE_LABEL_EOL;
        m_StateParamRequest = 0;
    };
};


void CProxyObj::SetProxyType(NetProxy_t Proxytype)
{
    m_ProxyType = Proxytype;
};


NetProxy_t CProxyObj::GetProxyType(void)
{
    return m_ProxyType;
};


void CProxyObj::SetProxyAddr(uint64 NetAddr)
{
    m_ProxyAddr = NetAddr;
};


uint64 CProxyObj::GetProxyAddr(void)
{
    return m_ProxyAddr;
};


void CProxyObj::StateRegist(int32 Label, IProxyObjState* pState, bool bFlagAlloc)
{
    ASSERT(pState);
    ASSERT(Label >= 0 && Label < COUNT_OF(m_aStatePtr));
    ASSERT(!m_aStatePtr[Label].Value);

    pState->m_pSubject = this;

    m_aStatePtr[Label].Value = pState;
    m_aStatePtr[Label].Parts.FlagAlloc = bFlagAlloc;
};


void CProxyObj::StateJump(int32 Label, void* Param)
{    
    if (Label == STATE_LABEL_EOL)
    {
		if (m_iStateLabelCurrent != STATE_LABEL_EOL)
		{
			m_aStatePtr[m_iStateLabelCurrent]->Detach();
			m_iStateLabelCurrent = Label;
		};
        Param = nullptr;        
        m_bFlagEol = true;
    };

    m_iStateLabelRequest = Label;
    m_StateParamRequest = Param;
};


void CProxyObj::ServiceRequest(uint32 Flag)
{
    m_uServiceFlags.fetch_or(Flag);
};


void* CProxyObj::GetShared(void)
{
    return m_SharedData;
};


void CProxyObj::SetShared(void* Param)
{
    m_SharedData = Param;
};


bool CProxyObj::IsEol(void) const
{
    return m_bFlagEol;
};


int32 CProxyObj::CurrentStateLabel(void) const
{
    return m_iStateLabelCurrent;
};