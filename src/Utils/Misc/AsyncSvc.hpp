#pragma once


class CAsyncSvc
{
public:    
    CAsyncSvc(const std::string& Label, uint32 UpdPeriod);
    virtual ~CAsyncSvc(void);
    virtual void OnStart(void) {};
    virtual void OnStop(void) {};
    virtual void OnRun(void) {};
    void Start(void);
    void Stop(void);

private:
    HOBJ m_hAsyncSvcObj;
};