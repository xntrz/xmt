#include "ProxySettings.hpp"


/*static*/ bool CProxySettings::IsMultithread()
{
    bool bResult = false;
    regset::load(bResult, "app_prxsys_threads");
    return bResult;
};