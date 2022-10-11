#include "TvlResult.hpp"
#include "TvlSettings.hpp"


/*static*/ CTvlResult& CTvlResult::Instance(void)
{
    static CTvlResult TvlResultInstance;
    return TvlResultInstance;
};


void CTvlResult::Start(void)
{
    CProxyResult::Start();

    m_nViewerCountWork = 0;
    m_nViewerCountReal = 0;
    m_Errtype = ERRTYPE_NONE;
    std::memset(m_aWalkTimestamps, 0x00, sizeof(m_aWalkTimestamps));
    m_nWalkTimestampsNum = 0;
    m_uWalktime = 0;
};


void CTvlResult::Stop(void)
{
    CProxyResult::Stop();
};


void CTvlResult::AddViewerCountWork(int32 nViewerCount)
{
    if (TvlSettings.RunMode != TvlRunMode_Keepalive)
        return;
    
    ASSERT((m_nViewerCountWork + nViewerCount) >= 0);
    m_nViewerCountWork += nViewerCount;
};


int32 CTvlResult::GetViewerCountWork(void)
{
    return m_nViewerCountWork;
};


void CTvlResult::SetViewerCountReal(int32 nViewerCount)
{
    m_nViewerCountReal = nViewerCount;
};


int32 CTvlResult::GetViewerCountReal(void)
{
    return m_nViewerCountReal;
};


void CTvlResult::SetError(ERRTYPE ErrorType)
{
    if (m_Errtype == ERRTYPE_NONE)
        m_Errtype = ErrorType;
};


CTvlResult::ERRTYPE CTvlResult::GetError(void)
{
    return m_Errtype;
};


void CTvlResult::SetCtxObjWalkTime(uint32 WalkTimeMS)
{
    if (m_nWalkTimestampsNum >= COUNT_OF(m_aWalkTimestamps))
    {
        uint32 Summ = 0;
        
        for (int32 i = 0; i < COUNT_OF(m_aWalkTimestamps); ++i)
            Summ += m_aWalkTimestamps[i];
        
        Summ /= COUNT_OF(m_aWalkTimestamps);
        
        m_uWalktime = Summ;
        m_nWalkTimestampsNum = 0;
    }
    else
    {
        m_aWalkTimestamps[m_nWalkTimestampsNum++] = WalkTimeMS;
    };
};


uint32 CTvlResult::GetCtxObjWalkTime(void) const
{
    return m_uWalktime;
};