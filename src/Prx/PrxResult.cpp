#include "PrxResult.hpp"


/*static*/ CPrxResult& CPrxResult::Instance(void)
{
    static CPrxResult PrxResultInstance;
    return PrxResultInstance;
};


void CPrxResult::Start(void)
{
    CProxyResult::Start();

    m_nNumFailTotal = 0;
    m_nNumSuccess = 0;
    m_nNumPending = 0;
};


void CPrxResult::Stop(void)
{
    CProxyResult::Stop();
};


void CPrxResult::NoteStart(void)
{
    ++m_nNumPending;
};


void CPrxResult::NoteStop(void)
{
    ASSERT(m_nNumPending > 0);
    --m_nNumPending;
};


void CPrxResult::NoteResult(RESULTTYPE Resulttype, uint64 NetAddr)
{
    switch (Resulttype)
    {
    case RESULTTYPE_OK:
        ++m_nNumSuccess;
        ExportList().insert(NetAddr);
        break;

    case RESULTTYPE_FAIL:
        ++m_nNumFailTotal;        
        break;

    default:
        ASSERT(false);
        break;
    };
};


int32 CPrxResult::GetNumFailTotal(void)
{
    return m_nNumFailTotal;
};


int32 CPrxResult::GetNumSuccess(void)
{
    return m_nNumSuccess;
};


int32 CPrxResult::GetNumPending(void)
{
    return m_nNumPending;
};


int32 CPrxResult::GetNumProcessed(void)
{
    return (m_nNumFailTotal + m_nNumSuccess);
};