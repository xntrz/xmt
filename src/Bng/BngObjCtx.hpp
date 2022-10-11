#pragma once

#include "Shared/Network/Net.hpp"

#include "Utils/Proxy/ProxyObj.hpp"
#include "Utils/Websocket/Websocket.hpp"
#include "Utils/Http/HttpReq.hpp"
#include "Utils/Misc/DataStore.hpp"
#include "Utils/Misc/Timer.hpp"


class CBngObjCtxShared;


class CBngStateBase : public IProxyObjState
{
public:
    virtual void Attach(void* Param) override {};
	virtual void Detach(void) override {};
    virtual void Observing(void) override {};
    CBngObjCtxShared& Shared(void);
    CHttpReq& Req(void);
    CDataStore& Ds(void);
    CWebsocket& Ws(void);
    std::string HostForMe(void);
    void RequestYield(void);
};


class CBngStateHostResolve final : public CBngStateBase
{
public:
    virtual void Attach(void* Param) override;
};


class CBngStateInitPhase1 final : public CBngStateBase
{
public:
    virtual void Attach(void* Param) override;
};


class CBngStateInitPhase2 final : public CBngStateBase
{
public:
    virtual void Attach(void* Param) override;
};


class CBngStateChat final : public CBngStateBase
{
private:
    enum STEP
    {
        STEP_JOIN = 0,
        STEP_CONNECT,
        STEP_PING,
        STEP_TRASH,
    };

public:
    virtual void Attach(void* Param) override final;
    virtual void Detach(void) override final;
    void SendPing(void);
    
private:
    int32 m_iMsgSequenceNo;
    STEP m_eStep;
};


class CBngObjCtxShared
{
public:
    CDataStore m_DataStore;
    CHttpReq* m_pReq;
    CWebsocket m_ws;
    std::string m_HostForMe;
    std::atomic<int32> m_RetryNo;
};


class CBngObjCtxTag
{
public:
    ;
};


class CBngObjCtx final :
    public CProxyObj,
    public CListNode<CBngObjCtx, CBngObjCtxTag>
{
public:
    static CQueue<uint64>* m_pProxyQueue;
    
    CBngObjCtx(void);
    virtual ~CBngObjCtx(void);
    virtual void Start(void) override;
    virtual void Stop(void) override;
    virtual void Service(uint32 uServiceFlags) override;
    bool IsRequestedYield(void) const;
    bool IsRequestedEol(void) const;
    void SetSharedReq(CHttpReq* pReq);
    bool StrideNextProxy(void);

private:
    CBngStateHostResolve m_StateHostResolve;
    CBngStateInitPhase1 m_StateInitPhase1;
    CBngStateInitPhase2 m_StateInitPhase2;
    CBngStateChat m_StateChat;
    CBngObjCtxShared m_Shared;
    int32 m_nStateSequenceCur;
    int32 m_nStateSequenceHead;
    int32 m_nStateSequenceTail;
    int32 m_nStateSequenceSave;
    CTimer m_SleepTimer;
    bool m_bRequestedYield;
    bool m_bInitFlag;
    bool m_bBotFlag;
};