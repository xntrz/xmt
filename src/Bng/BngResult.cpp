#include "BngResult.hpp"


/*static*/ CBngResult& CBngResult::Instance(void)
{
    static CBngResult BngResultInstance;
    return BngResultInstance;
};


void CBngResult::Start(void)
{
    CProxyResult::Start();

    m_nViewerCountWork = 0;
    m_nViewerCountReal = 0;
    m_Errtype = ERRTYPE_NONE;
    std::memset(m_aWalkTimestamps, 0x00, sizeof(m_aWalkTimestamps));
    m_nWalkTimestampsNum = 0;
    m_uWalktime;
};


void CBngResult::Stop(void)
{
    CProxyResult::Stop();
};


void CBngResult::AddViewerCountWork(int32 nViewerCount)
{
    ASSERT((m_nViewerCountWork + nViewerCount) >= 0);
    m_nViewerCountWork += nViewerCount;
};


int32 CBngResult::GetViewerCountWork(void)
{
    return m_nViewerCountWork;
};


void CBngResult::SetViewerCountReal(int32 nViewerCount)
{
    m_nViewerCountReal = nViewerCount;
};


int32 CBngResult::GetViewerCountReal(void)
{
    return m_nViewerCountReal;
};


void CBngResult::SetError(ERRTYPE ErrorType)
{
    if (m_Errtype == ERRTYPE_NONE)
        m_Errtype = ErrorType;
};


CBngResult::ERRTYPE CBngResult::GetError(void)
{
    return m_Errtype;
};


void CBngResult::SetCtxObjWalkTime(uint32 WalkTimeMS)
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


uint32 CBngResult::GetCtxObjWalkTime(void) const
{
    return m_uWalktime;
};