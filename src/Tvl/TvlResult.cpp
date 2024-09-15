#include "TvlResult.hpp"
#include "TvlSettings.hpp"


CTvlResult::CTvlResult()
: m_nViewerCount(0)
, m_nViewerCountReal(0)
, m_errtype(ERRTYPE_NONE)
{
    ;
};


void CTvlResult::OnAttach()
{
    m_errtype = ERRTYPE_NONE;
    m_nViewerCount = 0;
    m_nViewerCountReal = 0;
};


void CTvlResult::OnDetach()
{
    ;
};


void CTvlResult::OnStart()
{
    m_errtype = ERRTYPE_NONE;
    m_nViewerCount = 0;
    m_nViewerCountReal = 0;
};


void CTvlResult::OnStop()
{
    ;
};


/*extern*/ CTvlResult TvlResult;