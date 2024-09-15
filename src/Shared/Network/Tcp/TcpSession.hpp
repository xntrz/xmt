#pragma once

#include "../NetTypes.hpp"
#include "../NetProxy.hpp"
#include "../NetSession.hpp"


class CIoService;


class CTcpSession final : public CNetSession
{
public:
    CTcpSession(CIoService& iosvc, CTcpSession& parent, void* initcmd);
    CTcpSession(CIoService& iosvc, NETEVENTPROC evtproc, void* evtparam);
    virtual ~CTcpSession();
    virtual bool OnPreDispatchUserProc(int32 id, void* param, event_proc_args& args) override;
    virtual void OnUserRefEnd() override;
    virtual void OnRefEnd() override;

    CTcpSession* Accept();
    bool Connect(uint32 Ip, uint16 Port, uint32 TimeoutMS);
    bool CancelConnect();
    bool Listen(int32 Backlog, uint32 Ip, uint16 Port);
    uint32 Read(void* Buffer, uint32 BufferSize);
    uint32 Write(const void* Data, uint32 DataSize, bool DisconnectOnComplete);
    bool Disconnect();
    bool Resolve(const char* Hostname, uint32 Timeout);
    bool CancelResolve();
    void SetReadTimeout(uint32 TimeoutMS);
    void SetForceClose(bool State);
    bool IsConnected();
    void ProxySet(NETPROXY Proxytype, uint64 NetAddr, const NETPROXYPARAM* Parameter);
    void ProxyClear();
    void SecEnable(bool State);
    void SecSetHost(const char* Hostname);
    bool LayerAttach(int32 label);
    void LayerDetach(int32 label);

    /*inline*/ void SetWriteNotify(bool state);
    /*inline*/ bool IsRemote() const;
    /*inline*/ bool IsSyncMode() const;
    /*inline*/ uint64 GetNetAddr() const;
    /*inline*/ CIoService& GetIoService() const;
    
private:
    CIoService& m_iosvc;
    uint64 m_netaddr;
    int8 m_proxyLayerLabel;
    bool m_bRemote;
    bool m_bNotifySent;
    bool m_bSync;
};

inline void CTcpSession::SetWriteNotify(bool state) {
    m_bNotifySent = state;
};

inline bool CTcpSession::IsRemote() const {
    return m_bRemote;
};

inline bool CTcpSession::IsSyncMode() const {
    return m_bSync;
};

inline uint64 CTcpSession::GetNetAddr() const {
    return m_netaddr;
};

inline CIoService& CTcpSession::GetIoService() const {
    return m_iosvc;
};