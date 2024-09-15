#pragma once

#include "TcpLayer.hpp"

#include "../NetProxy.hpp"


class CTcpProxy final : public CTcpLayer
{
public:
    enum cmdtype {
        cmdtype_setparam = cmdtype_extend,
    };

    struct cmd_setparam {
        const NETPROXYPARAM* param;
        uint64 netaddr;

        inline cmd_setparam(const NETPROXYPARAM* _param, uint64 _netaddr)
            : param(_param), netaddr(_netaddr) {};
    };

private:
    using auth_callback = std::function<bool(int32, void*)>;

    enum socks5step {
        socks5step_hello = 0,
        socks5step_auth,
        socks5step_connect,
    };

public:
    CTcpProxy(CTcpSession& session, int32 label, NETPROXY type);
    virtual ~CTcpProxy();
    virtual bool EventProc(int32 id, void* param) override;
    virtual bool CmdProc(int32 id, void* param) override;

    bool TrySend(const void* data, std::size_t size);
    bool NotifyComplete();
    bool SendConnectSocks5();
    bool EventProcHttp(int32 id, void* param);
    bool EventProcSocks4(int32 id, void* param);
    bool EventProcSocks5(int32 id, void* param);
    uint32 makeProxyError(int32 code) const;

private:
    NETPROXY m_type;
    uint64 m_netaddr;
    uint64 m_netaddrOrg;
    NETPROXYPARAM m_param;
    auth_callback m_cbAuth;
    uint32 m_errcode;
    int16 m_step;
    bool m_bParamValid;
    bool m_bAuthComplete;
    uint32 m_elapsed;
};