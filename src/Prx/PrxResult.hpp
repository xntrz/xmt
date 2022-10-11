#pragma once

#include "Utils/Proxy/ProxyResult.hpp"


class CPrxResult final : public CProxyResult
{
public:
    enum RESULTTYPE
    {
        RESULTTYPE_OK = 0,
        RESULTTYPE_FAIL,
    };
    
public:
    static CPrxResult& Instance(void);
    
    virtual void Start(void) override;
    virtual void Stop(void) override;
    void NoteStart(void);
    void NoteStop(void);
    void NoteResult(RESULTTYPE Resulttype, uint64 NetAddr = 0);
    int32 GetNumFailTotal(void);    
    int32 GetNumSuccess(void);
    int32 GetNumPending(void);
    int32 GetNumProcessed(void);

private:
    std::atomic<int32> m_nNumFailTotal;
    std::atomic<int32> m_nNumSuccess;
    std::atomic<int32> m_nNumPending;
};