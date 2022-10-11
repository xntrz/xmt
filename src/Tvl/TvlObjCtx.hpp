#pragma once

#include "Shared/Network/Net.hpp"

#include "Utils/Proxy/ProxyObj.hpp"
#include "Utils/Misc/DataStore.hpp"
#include "Utils/Http/HttpReq.hpp"
#include "Utils/Websocket/Websocket.hpp"
#include "Utils/Misc/Timer.hpp"


class CTvlObjCtxShared;


class CTvlStateBase : public IProxyObjState
{
public:
    virtual void Attach(void* Param) override {};
    virtual void Detach(void) override {};
    virtual void Observing(void) override;
	void SetAsyncStatus(int32 iAsyncStatus);
	int32 GetAsyncStatus(void);
    CTvlObjCtxShared& Shared(void);
    CHttpReq& Req(void);
    CDataStore& Ds(void);
    CWebsocket& Ws(void);
};


class CTvlStateWatch final : public CTvlStateBase
{
public:
    virtual void Attach(void* Param) override;
};


class CTvlStateWsTicket final : public CTvlStateBase
{
public:
    virtual void Attach(void* Param) override;
};


class CTvlStateWsToken final : public CTvlStateBase
{
public:
    virtual void Attach(void* Param) override;
};


class CTvlStateWs final : public CTvlStateBase
{
public:
    virtual void Attach(void* Param) override;
    virtual void Detach(void) override;
    virtual void Observing(void) override;
    void SendPing(void);

private:
    CTimer m_PingTimer;
    bool m_bBotFlag;
};


class CTvlObjCtxShared
{
public:
    std::atomic<int32> m_iAsyncStatus;
    CDataStore m_DataStore;
    CWebsocket m_ws;
    CHttpReq m_req;
};


class CTvlObjCtxTag
{
public:
    ;
};


class CTvlObjCtx final :
    public CProxyObj,
    public CListNode<CTvlObjCtx, CTvlObjCtxTag>
{
public:
    static int32 TargetChannelId;

    CTvlObjCtx(void);
    virtual ~CTvlObjCtx(void);
    virtual void Start(void) override;
    virtual void Stop(void) override;
    virtual void Service(uint32 uServiceFlags) override;
    
private:
    CTvlStateWatch m_StateWatch;
    CTvlStateWsTicket m_StateWsTicket;
    CTvlStateWsToken m_StateWsToken;
    CTvlStateWs m_StateWs;
    CTvlObjCtxShared m_Shared;
    int32 m_nStateSequenceCur;
    int32 m_nStateSequenceHead;
    int32 m_nStateSequenceTail;
    int32 m_nStateSequenceSave;
    bool m_bInitFlag;
    bool m_bBotFlag;
};