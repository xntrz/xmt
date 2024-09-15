#pragma once

#include "Shared/Network/Net.hpp"

#include "Utils/Proxy/ProxyObj.hpp"
#include "Utils/Proxy/ProxySeq.hpp"
#include "Utils/Misc/Timer.hpp"
#include "Utils/Misc/DataStore.hpp"
#include "Utils/Http/HttpReq.hpp"
#include "Utils/Websocket/Websocket.hpp"


struct CTvlStateShared
{
    CDataStore datastore;
    CWebsocket websocket;
    CHttpReq request;
};


class CTvlStateBase : public CProxyObj::CState
{
public:
    virtual void Attach() override {};
    virtual void Detach() override {};
    virtual void Observing() override {};

    inline CTvlStateShared& shared() { return *reinterpret_cast<CTvlStateShared*>(Subject().GetShared()); };
    inline CHttpReq& request() { return shared().request; };
    inline CDataStore& datastore() { return shared().datastore; };
    inline CWebsocket& websocket() { return shared().websocket; };
};


class CTvlStateWsTicket final : public CTvlStateBase
{
public:
    virtual void Attach() override;
};


class CTvlStateWsToken final : public CTvlStateBase
{
public:
    virtual void Attach() override;
};


class CTvlStateWs final : public CTvlStateBase
{
public:
    virtual void Attach() override;
    virtual void Detach() override;
    virtual void Observing() override;
    void SendPing();

private:
    CTimer m_PingTimer;
    bool m_bBotFlag;
};


class CTvlObjCtx final : public CProxyObj
{
public:
    static int32 TargetChannelId;

    inline CTvlObjCtx() {};
    inline virtual ~CTvlObjCtx() {};

    virtual void Start() override;
    virtual void Stop() override;
    virtual void Service(uint32 uServiceFlags) override;
    void SetInitialDatastore();

private:
    CTvlStateWsTicket m_stateWsTicket;
    CTvlStateWsToken m_stateWsToken;
    CTvlStateWs m_stateWs;
    CTvlStateShared m_shared;
    CProxyObjSeq m_seq; 
};