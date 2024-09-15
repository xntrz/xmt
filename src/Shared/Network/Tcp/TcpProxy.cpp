#include "TcpProxy.hpp"

#include "../NetAddr.hpp"
#include "../NetTypes.hpp"


template<const uint32 buffsize>
class CBufferStream
{
public:
    inline CBufferStream(const void* buff, std::size_t size)
    : m_buff()
    , m_pos(0) {
        std::memcpy(m_buff, buff, std::min(size, buffsize));
    };

    inline CBufferStream()
    : m_buff()
    , m_pos(0) {
        ;
    };

    inline void skip(std::size_t bytes) {
        m_pos += bytes;
    };

    template<class data>
    inline void write(const data& d) {
        *reinterpret_cast<data*>(&m_buff[m_pos]) = d;
        m_pos += sizeof(data);
    };

    template<class data, int32 n>
    inline void write(const data(&d)[n]) {
        std::memcpy(&m_buff[m_pos], &d[0], sizeof(data) * n);
        m_pos += sizeof(data) * n;
    };

    inline void write(const void* data, std::size_t size) {
        std::memcpy(&m_buff[m_pos], data, size);
        m_pos += size;
    };

    template<class data>
    inline data read() {
        data& d = *reinterpret_cast<data*>(&m_buff[m_pos]);
        m_pos += sizeof(data);
        return d;
    };

    inline const void* data() const {
        return m_buff;
    };

    inline std::size_t size() const {
        return m_pos;
    };

private:
    char m_buff[buffsize];
    std::size_t m_pos;
};


CTcpProxy::CTcpProxy(CTcpSession& session, int32 label, NETPROXY type)
: CTcpLayer(session, label)
, m_type(type)
, m_netaddr(0)
, m_netaddrOrg(0)
, m_param({})
, m_cbAuth(nullptr)
, m_errcode(0)
, m_step(-1)
, m_bParamValid(false)
, m_bAuthComplete(false)
, m_elapsed(0)
{
    NetAddrInit(&m_netaddr);
    NetAddrInit(&m_netaddrOrg);

    static bool(CTcpProxy::*cbAuthProcs[])(int32, void*) = {
        &CTcpProxy::EventProcHttp,
        &CTcpProxy::EventProcSocks4,
        &CTcpProxy::EventProcSocks4,
        &CTcpProxy::EventProcSocks5,
    };

    ASSERT(m_type >= 0);
    ASSERT(m_type < COUNT_OF(cbAuthProcs));
    static_assert(COUNT_OF(cbAuthProcs) == NETPROXYNUM, "update me");

    m_cbAuth = std::bind(cbAuthProcs[m_type], this, std::placeholders::_1, std::placeholders::_2);
};


CTcpProxy::~CTcpProxy()
{
    ;
};


bool CTcpProxy::EventProc(int32 id, void* param)
{
    if (m_bAuthComplete)
        return SendEvent(id, param);

    bool bResult = false;

    switch (id) {
    case evttype_connect_fail:
        {
            evt_connect_fail& evt = *reinterpret_cast<evt_connect_fail*>(param);

            /* since we modifying connection address we should correct it if fail during transport call */
            evt.ip = NetAddrIp(&m_netaddrOrg);
            evt.port = NetAddrPort(&m_netaddrOrg);

            bResult = SendEvent(evttype_connect_fail, &evt);
        }
        break;

    case evttype_dc:
        {
            /* got dc but auth is not completed - morph disconnect event to connect fail event */
            bResult = SendEvent(evttype_connect_fail, &evt_connect_fail(NetAddrIp(&m_netaddrOrg), NetAddrPort(&m_netaddrOrg), m_errcode, Label()));
        }
        break;

    default:
        {
            if (id == evttype_connect)
                m_elapsed = (*static_cast<evt_connect*>(param)).elapsed;

            /* dispatch read/connect event in specific proxy type proc */
            bResult = m_cbAuth(id, param);
        }
        break;
    };

    return bResult;
};


bool CTcpProxy::CmdProc(int32 id, void* param)
{
    bool bResult = false;

    switch (id) {
    case cmdtype_connect:
        {
            /* proxy was set incorrect */
            if (m_netaddr == 0)
                break;

            /* intercept connect cmd and replace address to our proxy */
            cmd_connect& cmd = *reinterpret_cast<cmd_connect*>(param);

            /* init per connect data */
            m_bAuthComplete = false;
            m_step = -1;
            m_errcode = 0;
            m_elapsed = 0;

            /* save org address */
            NetAddrInit(&m_netaddrOrg, cmd.ip, cmd.port);

            /* set proxy address */
            cmd.ip = NetAddrIp(&m_netaddr);
            cmd.port = NetAddrPort(&m_netaddr);

            /* send cmd to next layer */
            bResult = SendCmd(id, &cmd);
        }
        break;

    case cmdtype_setparam:
        {
            cmd_setparam& cmd = *reinterpret_cast<cmd_setparam*>(param);

            m_netaddr = cmd.netaddr;
            m_bParamValid = (cmd.param != nullptr);
            if (m_bParamValid)
                m_param = *cmd.param;

            bResult = true;
        }
        break;

    default:
        bResult = SendCmd(id, param);
        break;
    };

    return bResult;
};


bool CTcpProxy::TrySend(const void* data, std::size_t size)
{
    return SendCmd(cmdtype_write, &cmd_write(data, size));
};


bool CTcpProxy::NotifyComplete()
{
    m_bAuthComplete = true;
    return SendEvent(evttype_connect, &evt_connect(NetAddrIp(&m_netaddrOrg), NetAddrPort(&m_netaddrOrg), m_elapsed, IsRemote()));
};


bool CTcpProxy::SendConnectSocks5()
{
    uint64 NetAddrTarget = m_netaddrOrg;
    NetAddrHton(&NetAddrTarget);
    
    CBufferStream<NETPROXY_BUFF_MAX> bs;
    bs.write<uint8>(NETPROXY_SOCKS5_VERSION);
    bs.write<uint8>(uint8(m_bParamValid ? m_param.Socks5.Command : NETPROXY_SOCKS5_COMMAND_TCP_CONNECTION));
    bs.write<uint8>(0x00); // RSV reserved, must be 0x00

    switch (m_param.Socks5.AddrType) {
    default:
    case NETPROXY_SOCKS5_ADDRTYPE_IPV4:
        {
            bs.write<uint8>(NETPROXY_SOCKS5_ADDRTYPE_IPV4);
            bs.write<uint32>(NetAddrIp(&NetAddrTarget));
        }
        break;

    case NETPROXY_SOCKS5_ADDRTYPE_IPV6:
        {
            bs.write<uint8>(NETPROXY_SOCKS5_ADDRTYPE_IPV6);
            bs.write(m_param.Socks5.Addr.IPv6);
        }
        break;

    case NETPROXY_SOCKS5_ADDRTYPE_DOMAIN:
        {
            int32 DomainLen = std::strlen(m_param.Socks5.Addr.Domain);

            bs.write<uint8>(NETPROXY_SOCKS5_ADDRTYPE_DOMAIN);
            bs.write<uint8>(DomainLen);
            bs.write(m_param.Socks5.Addr.Domain, DomainLen);
        }
        break;
    };

    bs.write<uint16>(NetAddrPort(&NetAddrTarget));

    return TrySend(bs.data(), bs.size());
};


bool CTcpProxy::EventProcHttp(int32 id, void* param)
{
    bool bResult = false;

    switch (id) {
    case evttype_connect:
        {
            char buffer[NETPROXY_BUFF_MAX];
            buffer[0] = '\0';

            std::size_t bytes = 0;

            if (m_bParamValid) {
                bytes += std::size_t(std::sprintf(
                    buffer,
                    "CONNECT %s:%hu HTTP/1.1\r\n"
                    "Host: %s:%hu\r\n"
                    "Proxy-Authorization: Basic %s:%s\r\n"                 /* TODO http auth base64 encode "user:password"*/
                    "\r\n",
                    NetAddrToStdStringIp(&m_netaddrOrg).c_str(), NetAddrPort(&m_netaddrOrg),
                    NetAddrToStdStringIp(&m_netaddrOrg).c_str(), NetAddrPort(&m_netaddrOrg),
                    m_param.Http.UserId, m_param.Http.UserPw
                ));
            }
            else {
                bytes += std::size_t(std::sprintf(
                    buffer,
                    "CONNECT %s:%hu HTTP/1.1\r\n"
                    "Host: %s:%hu\r\n"
                    "\r\n",
                    NetAddrToStdStringIp(&m_netaddrOrg).c_str(), NetAddrPort(&m_netaddrOrg),
                    NetAddrToStdStringIp(&m_netaddrOrg).c_str(), NetAddrPort(&m_netaddrOrg)
                ));
            };

            bResult = TrySend(buffer, bytes);
        }
        break;

    case evttype_read:
        {
            evt_read& evt = *reinterpret_cast<evt_read*>(param);

            int32 status = -1;
            static const char* patterns[] = { "HTTP/1.1 %d", "HTTP/1.0 %d", "HTTP/2.0 %d", };
            for (int32 i = 0; i < COUNT_OF(patterns); ++i) {
                if (std::sscanf(reinterpret_cast<const char*>(evt.buffer), patterns[i], &status) > 0)
                    break;
            };

            switch (status) {
            case 201:	// CREATED
            case 202:	// ACCEPTED
            case 203:	// NON-AUTHORATIVE INFORMATION
            case 204:	// NO CONTENT
            case 205:	// RESET CONTENT
            case 206:	// PATRIAL CONTENT
            case 207:	// MULTI-STATUS
            case 208:	// ALREADY REPORTED
            case 226:	// IM USED
            case 200:	// OK
                bResult = NotifyComplete();
                break;

            default:
                /* auth failed, pass status code as error code to connect fail event */
                m_errcode = makeProxyError(status);
                break;
            };
        }
        break;
    };

    return bResult;
};


bool CTcpProxy::EventProcSocks4(int32 id, void* param)
{
    bool bResult = false;

    switch (id)
    {
    case evttype_connect:
        {
            CBufferStream<NETPROXY_BUFF_MAX> bs;
            uint64 NetAddrTarget = m_netaddrOrg;

            if ((m_type == NETPROXY_SOCKS4A) &&
                (m_param.Socks4a.Domain[0] != '\0')) {
                //
                //  A server using protocol SOCKS4a must check the DSTIP in the request packet. 
                //  If it represents address 0.0.0.x with nonzero x, the server must read in 
                //  the domain name that the client sends in the packet. 
                //  The server should resolve the domain name and make connection to the destination host if it can.
                //
                NetAddrInit(&NetAddrTarget, "0.0.0.255", NetAddrPort(&NetAddrTarget));
                NetAddrHton(&NetAddrTarget);
            }
            else {
                NetAddrHton(&NetAddrTarget);
            };

            bs.write<uint8>(NETPROXY_SOCKS4_VERSION);
            bs.write<uint8>(NETPROXY_SOCKS4_CMD_CONNECT);
            bs.write<uint16>(NetAddrPort(&NetAddrTarget));
            bs.write<uint32>(NetAddrIp(&NetAddrTarget));

            if (m_bParamValid) {
                if (m_param.Socks4.identd[0] != '\0')
                    bs.write(m_param.Socks4.identd, std::strlen(m_param.Socks4.identd));

                bs.write<uint8>('\0');

                if (m_type == NETPROXY_SOCKS4A) {
                    if (m_param.Socks4a.Domain[0] != '\0')
                        bs.write(m_param.Socks4a.Domain, std::strlen(m_param.Socks4a.Domain));
                    bs.write<uint8>('\0');
                };
            }
            else {
                bs.write<uint8>('\0');
            };

            bResult = TrySend(bs.data(), bs.size());
        }
        break;

    case evttype_read:
        {
            evt_read& evt = *reinterpret_cast<evt_read*>(param);
            CBufferStream<NETPROXY_BUFF_MAX> bs(evt.buffer, evt.bytes);
            
            if (evt.bytes >= 2) {
                uint8 Empty = bs.read<uint8>();
                uint8 Code = bs.read<uint8>();
                if (Empty == 0) {
                    switch (Code) {
                    case NETPROXY_SOCKS4_STATUS_GRANTED:
                        bResult = NotifyComplete();
                        break;
                    default:
                        m_errcode = makeProxyError(Code);
                        break;
                    };
                };
            };
        }
        break;
    };

    return bResult;
};


bool CTcpProxy::EventProcSocks5(int32 id, void* param)
{
    bool bResult = false;

    switch (id) {
    case evttype_connect:
        {
            CBufferStream<NETPROXY_BUFF_MAX> bs;
            bs.write<uint8>(NETPROXY_SOCKS5_VERSION);

            if (m_bParamValid) {
                bs.write<uint8>(m_param.Socks5.AuthtypeNum);
                for (int32 i = 0; i < m_param.Socks5.AuthtypeNum; ++i)
                    bs.write<uint8>(m_param.Socks5.Authtype[i]);
            }
            else {
                /* If not parameter present, assume there is 1 method with no auth type */
                bs.write<uint8>(1);
                bs.write<uint8>(NETPROXY_SOCKS5_AUTHTYPE_NO_AUTH);
            };

            m_step = socks5step_hello;
            bResult = TrySend(bs.data(), bs.size());
        }
        break;

    case evttype_read:
        {
            evt_read& evt = *reinterpret_cast<evt_read*>(param);
            if (evt.bytes < 2) {
                OUTPUTLN("socks5 got read that less than 2 bytes");
                break;
            };

            CBufferStream<NETPROXY_BUFF_MAX> bs(evt.buffer, evt.bytes);

            switch (m_step) {
            case socks5step_hello:
                {
                    uint8 Ver = bs.read<uint8>();
                    uint8 Cauth = bs.read<uint8>();

                    switch (Cauth) {
                    case NETPROXY_SOCKS5_AUTHTYPE_NO_AUTH:
                        m_step = socks5step_connect;
                        bResult = SendConnectSocks5();
                        break;

                    case NETPROXY_SOCKS5_AUTHTYPE_USERNAME_PASSWORD:
                        if (m_bParamValid) {
                            int32 IdLen = std::strlen(m_param.Socks5.UserId);
                            int32 PwLen = std::strlen(m_param.Socks5.UserPw);

                            CBufferStream<NETPROXY_BUFF_MAX> bsw;
                            bsw.write<uint8>(0x01);
                            bsw.write<uint8>(IdLen);
                            bsw.write(m_param.Socks5.UserId, IdLen);
                            bsw.write<uint8>(PwLen);
                            bsw.write(m_param.Socks5.UserPw, PwLen);

                            m_step = socks5step_auth;
                            bResult = TrySend(bsw.data(), bsw.size());
                        }
                        else {
                            m_errcode = makeProxyError(Cauth);
                        };
                        break;

                    case NETPROXY_SOCKS5_AUTHTYPE_GSSAPI:
                        OUTPUTLN("TODO: authtype GSSAPI");
                    default:
                        m_errcode = makeProxyError(Cauth);
                        break;
                    };
                }
                break;

            case socks5step_auth:
                {
                    ASSERT(m_bParamValid == true);

                    uint8 Ver = bs.read<uint8>();
                    uint8 Status = bs.read<uint8>();

                    switch (Status) {
                    case 0x00:
                        m_step = socks5step_connect;
                        bResult = SendConnectSocks5();
                        break;

                    default:
                        m_errcode = makeProxyError(Status);
                        break;
                    };
                }
                break;

            case socks5step_connect:
                {
                    uint8 Ver = bs.read<uint8>();
                    uint8 Status = bs.read<uint8>();

                    switch (Status) {
                    case NETPROXY_SOCKS5_STATUS_GRANTED:
                        bResult = NotifyComplete();
                        break;
                    case NETPROXY_SOCKS5_STATUS_FAILURE:
                    case NETPROXY_SOCKS5_STATUS_NOT_ALLOWED:
                    case NETPROXY_SOCKS5_STATUS_NETWORK_UNREACHABLE:
                    case NETPROXY_SOCKS5_STATUS_HOST_UNREACHABLE:
                    case NETPROXY_SOCKS5_STATUS_REFUSED:
                    case NETPROXY_SOCKS5_STATUS_TTL_EXPIRED:
                    case NETPROXY_SOCKS5_STATUS_CMD_NOT_SUPPORTED:
                    case NETPROXY_SOCKS5_STATUS_ADDR_NOT_SUPPORTED:
                    default:
                        m_errcode = makeProxyError(Status);
                        break;
                    };
                };
                break;

            default:
                ASSERT(false);
                break;
            };
        }
        break;
    };

    return bResult;
};


uint32 CTcpProxy::makeProxyError(int32 code) const
{
    uint32 errcode = 0;

    switch (m_type) {
    case NETPROXY_HTTP:
    case NETPROXY_SOCKS4:
    case NETPROXY_SOCKS4A:
        errcode = uint32(code);
        break;
    case NETPROXY_SOCKS5:
        /* hiword is step, loword is code */
        errcode = ((m_step << 16) | (code & 0x0000FFFF));
        break;
    default:
        ASSERT(false);
        break;
    };

    return errcode;
};