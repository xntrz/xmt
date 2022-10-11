#pragma once

#include "ProxyList.hpp"


class CProxyResult
{
public:
    CProxyResult(void);
    virtual ~CProxyResult(void);
    virtual void Attach(void);
    virtual void Detach(void);
    virtual void Start(void);
    virtual void Stop(void);
    CProxyList& ImportList(void);
    CProxyList& ExportList(void);

protected:
    CProxyList m_ProxyListImport;
    CProxyList m_ProxyListExport;
};