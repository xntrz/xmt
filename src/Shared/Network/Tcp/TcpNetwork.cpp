#include "TcpNetwork.hpp"
#include "TcpSession.hpp"
#include "TcpTransportAct.hpp"

#include "../NetSsl.hpp"


CTcpNetwork::CTcpNetwork()
: m_iosvc(std::thread::hardware_concurrency() / 2u, "TcpIoSvc")
, m_resolverCache()
{
    m_iosvc.on_thread_exit([]() {
        CNetSsl::ThreadCleanup();
    });
};


CTcpNetwork::~CTcpNetwork()
{
    m_iosvc.stop_and_wait(std::chrono::seconds(5));
};


HCONN CTcpNetwork::Open(NETEVENTPROC EventProc, void* UserParam)
{
    return new CTcpSession(m_iosvc, EventProc, UserParam);
};


void CTcpNetwork::Close(HCONN hConn)
{
    Session(hConn).Close();
};


HCONN CTcpNetwork::Copy(HCONN hConn)
{
    return Session(hConn).Copy();
};


HCONN CTcpNetwork::Accept(HCONN hConn)
{
    return Session(hConn).Accept();
};


bool CTcpNetwork::Connect(HCONN hConn, uint32 Ip, uint16 Port, uint32 Timeout)
{
    return Session(hConn).Connect(Ip, Port, Timeout);
};


bool CTcpNetwork::CancelConnect(HCONN hConn)
{
    return Session(hConn).CancelConnect();
};


bool CTcpNetwork::Listen(HCONN hConn, int32 Backlog, uint32 Ip, uint16 Port)
{
    return Session(hConn).Listen(Backlog, Ip, Port);
};


uint32 CTcpNetwork::Read(HCONN hConn, void* Buff, uint32 BuffSize)
{
    return Session(hConn).Read(Buff, BuffSize);
};


uint32 CTcpNetwork::Write(HCONN hConn, const void* Data, uint32 DataSize, bool FlagDisconnectOnComplete)
{
    return Session(hConn).Write(Data, DataSize, FlagDisconnectOnComplete);
};


bool CTcpNetwork::Disconnect(HCONN hConn)
{
    return Session(hConn).Disconnect();
};


bool CTcpNetwork::Resolve(HCONN hConn, const char* Hostname, uint32 Timeout)
{
    return Session(hConn).Resolve(Hostname, Timeout);
};


bool CTcpNetwork::CancelResolve(HCONN hConn)
{
    return Session(hConn).CancelResolve();
};


void CTcpNetwork::SetProxy(HCONN hConn, NETPROXY Proxytype, uint64 NetAddr, const NETPROXYPARAM* Parameter)
{
    Session(hConn).ProxySet(Proxytype, NetAddr, Parameter);
};


void CTcpNetwork::ClearProxy(HCONN hConn)
{
    Session(hConn).ProxyClear();
};


void CTcpNetwork::SetSecurity(HCONN hConn, bool Flag)
{
    Session(hConn).SecEnable(Flag);
};


void CTcpNetwork::SetSecurityHost(HCONN hConn, const char* Hostname)
{
    Session(hConn).SecSetHost(Hostname);
};


void CTcpNetwork::SetTimeoutRead(HCONN hConn, uint32 Timeout)
{
    Session(hConn).SetReadTimeout(Timeout);
};


void CTcpNetwork::SetForceClose(HCONN hConn, bool Flag)
{
    Session(hConn).SetForceClose(Flag);
};


void CTcpNetwork::SetUserParam(HCONN hConn, void* UserParam)
{
    Session(hConn).SetUserParam(UserParam);
};


void* CTcpNetwork::GetUserParam(HCONN hConn)
{
    return Session(hConn).GetUserParam();
};


bool CTcpNetwork::IsRemote(HCONN hConn)
{
    return Session(hConn).IsRemote();
};


bool CTcpNetwork::IsConnected(HCONN hConn)
{
    return Session(hConn).IsConnected();
};


uint64 CTcpNetwork::NetAddr(HCONN hConn)
{
    return Session(hConn).GetNetAddr();
};


void CTcpNetwork::SetEventProc(HCONN hConn, NETEVENTPROC EventProc, void* Param)
{
    Session(hConn).SetEventProc(EventProc, Param);
};


void CTcpNetwork::GetEventProc(HCONN hConn, NETEVENTPROC* EventProc, void** Param)
{
    Session(hConn).GetEventProc(*EventProc, *Param);
};


bool CTcpNetwork::GetAddrinfo(const char* Hostname, uint32* IpAddrArray, int32* IpAddrArrayCount)
{
    std::vector<uint32> addresses;
    addresses.reserve(*IpAddrArrayCount);
    
    /* check cache for host */
    if (m_resolverCache.lookup(Hostname, addresses))
    {
        int32 Cnt = std::min(*IpAddrArrayCount, int32(addresses.size()));
        for (int32 i = 0; i < Cnt; ++i)
            IpAddrArray[i] = addresses[i];
		*IpAddrArrayCount = Cnt;
        return (addresses.size() > 0);
    };

    CEvent evt;
    
    /* cache miss for hostname or record timeout resolve it again */
    NETEVENTPROC netEventCallback = [&](HCONN hConn, NETEVENT Event, uint32 Error, uint64 NetAddr, const void* Data, uint32 DataSize, void* Param) {
        switch (Event) {
        case NETEVENT_RESOLVE:
            {
                if (!Error)
                {
                    int32 Cnt = std::min(*IpAddrArrayCount, int32(DataSize));
                    for (int32 i = 0; i < Cnt; ++i)
                    {
                        IpAddrArray[i] = reinterpret_cast<const uint32*>(Data)[i];
                        addresses.push_back(IpAddrArray[i]);
                    };
                    *IpAddrArrayCount = Cnt;
                };
                evt.signal_all();
            }
            break;
        };
        return true;
    };
    
    HCONN hDummy = Open(netEventCallback);
    if (hDummy)
    {
        if (Resolve(hDummy, Hostname, 60000))
            evt.wait_for(std::chrono::seconds(60));
        Close(hDummy);

        if (!addresses.empty()) 
        {
            /**
             *  multiple threads may be missed for hostname at first cache lookup and perform multiple resolve same host at the same time
             *  write func performs double check and return this status just for debug
             */
            std::chrono::milliseconds cacheTimeout(60000 * 3);    // 3 min
            bool doubleCheckStatus = m_resolverCache.write(Hostname, addresses, cacheTimeout);

#ifdef _DEBUG
            OUTPUTLN(
                "RESOLVER: host \"%s\" is missed, caching it now for %" PRIu32 " seconds (double check result: %" PRIi32 ")",
                Hostname,
                uint32(cacheTimeout.count() / 1000),
                int32(doubleCheckStatus)
            );
#endif        
        };        
    };
    
    return (addresses.size() > 0);
};


bool CTcpNetwork::GetAddrinfo(const char* Hostname, uint32* Ip)
{
    uint32 IpAddrArray[8] = { 0 };
    int32 IpAddrArrayCnt = COUNT_OF(IpAddrArray);

    if (GetAddrinfo(Hostname, IpAddrArray, &IpAddrArrayCnt))
    {
        *Ip = IpAddrArray[0];
        return true;
    };

    return false;
};


bool CTcpNetwork::GetAddrinfoSelf(uint32* IpAddrArray, int32* IpAddrArrayCount)
{
    return GetAddrinfo("localhost", IpAddrArray, IpAddrArrayCount);
};