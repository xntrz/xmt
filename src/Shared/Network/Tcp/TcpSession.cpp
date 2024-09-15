#include "TcpSession.hpp"

#include "TcpLayer.hpp"
#include "TcpTransport.hpp"
#include "TcpProxy.hpp"
#include "TcpSsl.hpp"
#include "TcpSync.hpp"

#include "../NetAddr.hpp"
#include "../NetSession.hpp"


/**
 *  Layer label order is important!
 *  All events goes from bottom layer to top (0 -> num)
 *  All commands goes from top layer to bottom (num -> 0)
 *  All layers manually resend events or cmd to the following layer
 *  Any layer can modify or delay command or event
 */
enum layerlabel {
    layerlabel_transport = 0,
    layerlabel_prx_http,
    layerlabel_prx_socks4,
    layerlabel_prx_socks4a,
    layerlabel_prx_socks5,
    layerlabel_ssl,
    layerlabel_sync,
    
    layerlabelnum,
    layerlabel_none = -1,
};


CTcpSession::CTcpSession(CIoService& iosvc, CTcpSession& parent, void* initcmd)
: CNetSession(parent)
, m_iosvc(iosvc)
, m_netaddr(0)
, m_proxyLayerLabel(layerlabel_none)
, m_bRemote(true)
, m_bNotifySent(false)
, m_bSync(false)
{
    NetAddrInit(&m_netaddr);

    LayerRefInc();
    
    UserRefInc();
    
    LayerAttach(layerlabel_transport);

    if (parent.LayerIsAttached(layerlabel_ssl))
        LayerAttach(layerlabel_ssl);

    if (parent.LayerIsAttached(layerlabel_sync))
        LayerAttach(layerlabel_sync);

    SendCmd(layerlabel_transport, CTcpTransport::cmdtype_complete_accept, initcmd);
};


CTcpSession::CTcpSession(CIoService& iosvc, NETEVENTPROC evtproc, void* evtparam)
: CNetSession(evtproc, evtparam)
, m_iosvc(iosvc)
, m_netaddr(0)
, m_proxyLayerLabel(layerlabel_none)
, m_bRemote(false)
, m_bNotifySent(false)
, m_bSync(false)
{
    NetAddrInit(&m_netaddr);
    
    UserRefInc();
    
    LayerAttach(layerlabel_transport);
    
    if (evtproc == nullptr)
        LayerAttach(layerlabel_sync);
};


CTcpSession::~CTcpSession()
{
    ;
};


bool CTcpSession::OnPreDispatchUserProc(int32 id, void* param, event_proc_args& args)
{
    if (m_bSync) {
        if (id == CTcpLayer::evttype_connect) {
            CTcpLayer::evt_connect& evt = *reinterpret_cast<CTcpLayer::evt_connect*>(param);
            NetAddrInit(&m_netaddr, evt.ip, evt.port);
        };
        return true;
    };
    
    args.dispatch = true;

    switch (id) {
    case CTcpLayer::evttype_accept:
        {
            args.dispatch = false;            
        }
        break;
        
    case CTcpLayer::evttype_connect:
        {
            CTcpLayer::evt_connect& evt = *reinterpret_cast<CTcpLayer::evt_connect*>(param);

            args.event = NETEVENT_CONNECT;
            NetAddrInit(&args.netaddr, evt.ip, evt.port);

            m_netaddr = args.netaddr;
        }
        break;

    case CTcpLayer::evttype_connect_fail:
        {
            CTcpLayer::evt_connect_fail& evt = *reinterpret_cast<CTcpLayer::evt_connect_fail*>(param);

            static NETEVENT LabelToEventTbl[] = {
                NETEVENT_CONNECTFAIL,       // tcp
                NETEVENT_CONNECTFAILPRX,    // http
                NETEVENT_CONNECTFAILPRX,    // s4
                NETEVENT_CONNECTFAILPRX,    // s4a
                NETEVENT_CONNECTFAILPRX,    // s5
                NETEVENT_CONNECTFAILSSL,    // ssl
                NETEVENT_CONNECTFAIL,       // sync
            };

            ASSERT(evt.label >= 0);
            ASSERT(evt.label < COUNT_OF(LabelToEventTbl));
            static_assert(layerlabelnum == COUNT_OF(LabelToEventTbl), "update table");

            args.event      = LabelToEventTbl[evt.label];
            args.errcode    = evt.errcode;
            NetAddrInit(&args.netaddr, evt.ip, evt.port);

            m_netaddr = args.netaddr;
        }
        break;

    case CTcpLayer::evttype_write:
        {
            CTcpLayer::evt_write& evt = *reinterpret_cast<CTcpLayer::evt_write*>(param);

            args.event      = NETEVENT_SEND;
            args.data       = evt.data;
            args.size       = evt.bytes;
            args.dispatch   = (m_bNotifySent ? true : false);
        }
        break;

    case CTcpLayer::evttype_read:
        {
            CTcpLayer::evt_read& evt = *reinterpret_cast<CTcpLayer::evt_read*>(param);

            args.event  = NETEVENT_RECV;
            args.data   = evt.buffer;
            args.size   = evt.bytes;
        }
        break;

    case CTcpLayer::evttype_dc:
        {
            CTcpLayer::evt_dc& evt = *reinterpret_cast<CTcpLayer::evt_dc*>(param);
            
            args.event = NETEVENT_DISCONNECT;
        }
        break;

    case CTcpTransport::evttype_resolve:
        {
            CTcpTransport::evt_resolve& evt = *reinterpret_cast<CTcpTransport::evt_resolve*>(param);

            args.event  = NETEVENT_RESOLVE;
            args.errcode= evt.errcode;
            args.data   = evt.ips.data();
            args.size   = evt.ips.size();
        }
        break;

    default:
        ASSERT(false);
        break;
    };

    return true;
};


void CTcpSession::OnUserRefEnd()
{
    SendCmd(layerlabel_transport, CTcpTransport::cmdtype_close, &CTcpTransport::cmd_close());
};


void CTcpSession::OnRefEnd()
{
    delete this;
};


CTcpSession* CTcpSession::Accept()
{
    CTcpSync::cmd_accept cmd;
    SendCmd(layerlabel_sync, CTcpSync::cmdtype_accept, &cmd);
    return cmd.session;
};


bool CTcpSession::Connect(uint32 Ip, uint16 Port, uint32 TimeoutMS)
{
    return SendCmd(CTcpLayer::cmdtype_connect, &CTcpLayer::cmd_connect(Ip, Port, TimeoutMS));
};


bool CTcpSession::CancelConnect()
{
    return SendCmd(CTcpLayer::cmdtype_cancel_connect, &CTcpLayer::cmd_cancel_connect());
};


bool CTcpSession::Listen(int32 Backlog, uint32 Ip, uint16 Port)
{
    return SendCmd(CTcpLayer::cmdtype_listen, &CTcpLayer::cmd_listen(Ip, Port, Backlog));
};


uint32 CTcpSession::Read(void* Buff, uint32 BuffSize)
{
    CTcpSync::cmd_read cmd(Buff, BuffSize);
    SendCmd(layerlabel_sync, CTcpSync::cmdtype_read, &cmd);
    return cmd.readed;
};


uint32 CTcpSession::Write(const void* Data, uint32 DataSize, bool DisconnectOnComplete)
{
    CTcpLayer::cmd_write cmd(Data, DataSize, DisconnectOnComplete);
    SendCmd(CTcpLayer::cmdtype_write, &cmd);
    return cmd.written;
};


bool CTcpSession::Disconnect()
{
    return SendCmd(CTcpLayer::cmdtype_dc, &CTcpLayer::cmd_dc());
};


bool CTcpSession::Resolve(const char* Hostname, uint32 Timeout)
{
    return SendCmd(layerlabel_transport, CTcpTransport::cmdtype_resolve, &CTcpTransport::cmd_resolve(Hostname, Timeout));
};


bool CTcpSession::CancelResolve()
{
    return SendCmd(layerlabel_transport, CTcpTransport::cmdtype_cancel_resolve, &CTcpTransport::cmd_cancel_resolve());
};


void CTcpSession::SetReadTimeout(uint32 TimeoutMS)
{
    SendCmd(layerlabel_transport, CTcpTransport::cmdtype_set_read_timeout, &CTcpTransport::cmd_set_param(TimeoutMS));
};


void CTcpSession::SetForceClose(bool State)
{
    if (!IsConnected())
        SendCmd(layerlabel_transport, CTcpTransport::cmdtype_set_force_close, &CTcpTransport::cmd_set_param(State));
};


bool CTcpSession::IsConnected()
{
    return SendCmd(layerlabel_transport, CTcpTransport::cmdtype_check_open, &CTcpTransport::cmd_check_open());
};


void CTcpSession::ProxySet(NETPROXY Proxytype, uint64 NetAddr, const NETPROXYPARAM* Parameter)
{
    if (IsConnected())
        return;

    if (LayerIsAnyAttached({ layerlabel_prx_http, layerlabel_prx_socks4, layerlabel_prx_socks4a, layerlabel_prx_socks5 }))
        return;

    const int32 proxytype2layerlabel[] = {
        layerlabel_prx_http,
        layerlabel_prx_socks4,
        layerlabel_prx_socks4a,
        layerlabel_prx_socks5
    };

    ASSERT(Proxytype >= 0);
    ASSERT(Proxytype < COUNT_OF(proxytype2layerlabel));

    if (LayerAttach(proxytype2layerlabel[Proxytype])) {
        ASSERT(m_proxyLayerLabel == layerlabel_none);
        m_proxyLayerLabel = proxytype2layerlabel[Proxytype];
        if (!SendCmd(m_proxyLayerLabel, CTcpProxy::cmdtype_setparam, &CTcpProxy::cmd_setparam(Parameter, NetAddr))) {
            LayerDetach(m_proxyLayerLabel);
            m_proxyLayerLabel = layerlabel_none;
        };
    };
};


void CTcpSession::ProxyClear()
{
    if (!IsConnected()) {
        if (m_proxyLayerLabel != layerlabel_none) {
            LayerDetach(m_proxyLayerLabel);
            m_proxyLayerLabel = layerlabel_none;
        };
    };    
};


void CTcpSession::SecEnable(bool State)
{
    if (!IsConnected())
        (State ? LayerAttach(layerlabel_ssl) : LayerDetach(layerlabel_ssl));
};


void CTcpSession::SecSetHost(const char* Hostname)
{
    if (!IsConnected())
        SendCmd(layerlabel_ssl, CTcpSsl::cmdtype_sethostname, &CTcpSsl::cmd_sethostname(Hostname));
};


bool CTcpSession::LayerAttach(int32 label)
{
    if (LayerIsAttached(label))
        return false;

    std::shared_ptr<CNetLayer> layer = nullptr;
    switch (label) {
    case layerlabel_transport:
        layer = std::make_shared<CTcpTransport>(*this, label);
        break;
    case layerlabel_prx_http:
        layer = std::make_shared<CTcpProxy>(*this, label, NETPROXY_HTTP);
        break;
    case layerlabel_prx_socks4:
        layer = std::make_shared<CTcpProxy>(*this, label, NETPROXY_SOCKS4);
        break;
    case layerlabel_prx_socks4a:
        layer = std::make_shared<CTcpProxy>(*this, label, NETPROXY_SOCKS4A);
        break;
    case layerlabel_prx_socks5:
        layer = std::make_shared<CTcpProxy>(*this, label, NETPROXY_SOCKS5);
        break;
    case layerlabel_ssl:
        layer = std::make_shared<CTcpSsl>(*this, label);
        break;
    case layerlabel_sync:
        layer = std::make_shared<CTcpSync>(*this, label);
        m_bSync = true;
        m_bNotifySent = true;
        break;
    default:
        ASSERT(false);
        break;
    };

    if (layer) {
        CNetSession::LayerAttach(label, layer);
        return true;
    };

    return false;
};


void CTcpSession::LayerDetach(int32 label)
{
    if (LayerIsAttached(label))
        CNetSession::LayerDetach(label);    
};