#include "NetSockUtils.hpp"
#include "NetAddr.hpp"

#include "Shared/Common/Time.hpp"


static const uint32 NET_DNS_CACHE_TIMEOUT = 60000 * 7; // 7 min


struct NetDnsResolveData_t
{
    uint32 Address;
    uint32 TimeWhenResolved;
};


struct NetDns_t
{
    std::unordered_map<std::string, NetDnsResolveData_t> CacheMap;
    std::recursive_mutex Mutex;
};


struct NetWsaContainer_t
{
    WSADATA WsaData;
    std::atomic<int32> SockPoolRef;
    std::atomic<int32> SockRef;
};


struct NetSockPool_t
{
    CQueue<SOCKET>* Queue;
    std::recursive_mutex Mutex;
};


static NetWsaContainer_t NetWsaContainer;
static NetDns_t* NetDns;


/*extern*/ LPFN_ACCEPTEX NetWsaFnAcceptEx = nullptr;
/*extern*/ LPFN_CONNECTEX NetWsaFnConnectEx = nullptr;
/*extern*/ LPFN_DISCONNECTEX NetWsaFnDisconnectEx = nullptr;
/*extern*/ LPFN_GETACCEPTEXSOCKADDRS NetWsaFnGetAcceptExSockAddrs = nullptr;


void NetWsaInitialize(void)
{
    NetDns = new NetDns_t;
    ASSERT(NetDns);
};


void NetWsaTerminate(void)
{
    ASSERT(NetWsaContainer.SockRef == 0);
    ASSERT(NetWsaContainer.SockPoolRef == 0);

    if (NetDns)
    {
        delete NetDns;
        NetDns = nullptr;
    };
};


bool NetWsaStart(void)
{
    if (WSAStartup(MAKEWORD(2, 2), &NetWsaContainer.WsaData))
        return false;

    auto FnLoadExFunction = [&](GUID fnGuid, LPVOID lpResult) -> void
    {
        SOCKET hDummySocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (hDummySocket)
        {
            DWORD cbTransfered = 0;

            WSAIoctl(
                hDummySocket,
                SIO_GET_EXTENSION_FUNCTION_POINTER,
                LPVOID(&fnGuid), sizeof(GUID),
                LPVOID(lpResult), sizeof(LPVOID),
                &cbTransfered,
                NULL,
                NULL
            );

            closesocket(hDummySocket);
        };
    };

    FnLoadExFunction(WSAID_ACCEPTEX, &NetWsaFnAcceptEx);
    FnLoadExFunction(WSAID_CONNECTEX, &NetWsaFnConnectEx);
    FnLoadExFunction(WSAID_DISCONNECTEX, &NetWsaFnDisconnectEx);
    FnLoadExFunction(WSAID_GETACCEPTEXSOCKADDRS, &NetWsaFnGetAcceptExSockAddrs);

    if (!NetWsaFnAcceptEx ||
        !NetWsaFnConnectEx ||
        !NetWsaFnDisconnectEx ||
        !NetWsaFnGetAcceptExSockAddrs)
        return false;

    return true;
};


void NetWsaStop(void)
{
    NetWsaFnGetAcceptExSockAddrs = nullptr;
    NetWsaFnDisconnectEx = nullptr;
    NetWsaFnConnectEx = nullptr;
    NetWsaFnAcceptEx = nullptr;
    WSACleanup();
};


SOCKET NetSockNewListen(uint64 NetAddr)
{
    SOCKET hSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
    if (hSock == INVALID_SOCKET)
    {
        OUTPUTLN("WSASocket() failed %u", WSAGetLastError());
        return INVALID_SOCKET;
    };

    ++NetWsaContainer.SockRef;

    sockaddr_in SockAddr = {};
    SockAddr.sin_family     = AF_INET;
    SockAddr.sin_port       = NetAddrPort(&NetAddr);
    SockAddr.sin_addr.s_addr= NetAddrIp(&NetAddr);

    if (bind(hSock, (const sockaddr*)&SockAddr, sizeof(SockAddr)) == SOCKET_ERROR)
    {
        OUTPUTLN("bind() failed %u", WSAGetLastError());
        NetSockClose(hSock);
        return INVALID_SOCKET;
    };

    if (listen(hSock, uint16_max) == SOCKET_ERROR)
    {
        OUTPUTLN("listen() failed %u", WSAGetLastError());
        NetSockClose(hSock);
        return INVALID_SOCKET;
    };

    return hSock;
};


SOCKET NetSockNewAccept(void)
{
    SOCKET hSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
    if (hSock == INVALID_SOCKET)
    {
        OUTPUTLN("WSASocket() failed %u", WSAGetLastError());
        return INVALID_SOCKET;
    };
    
    ++NetWsaContainer.SockRef;
    
    return hSock;
};


SOCKET NetSockNewConnect(void)
{
    SOCKET hSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
    if (hSock == INVALID_SOCKET)
    {
        OUTPUTLN("WSASocket() failed %u", WSAGetLastError());
        return INVALID_SOCKET;
    };

    ++NetWsaContainer.SockRef;

    if (!NetSockSetReuseAddr(hSock, true))
    {
        OUTPUTLN("NetSockSetReuseAddr() failed %u", WSAGetLastError());
        NetSockClose(hSock);
        return INVALID_SOCKET;
    };
    
    if (!NetSockSetPortScale(hSock, true))
    {
        OUTPUTLN("NetSockSetPortScale() failed %u", WSAGetLastError());
        NetSockClose(hSock);
        return INVALID_SOCKET;
    };
    
    sockaddr_in addr;
    memset(&addr, 0x00, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(hSock, (const sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        OUTPUTLN("bind() failed %u", WSAGetLastError());
        NetSockClose(hSock);
        return INVALID_SOCKET;
    };

    return hSock;
};


void NetSockClose(SOCKET hSock)
{
    ASSERT(hSock != INVALID_SOCKET);
    
    ASSERT(NetWsaContainer.SockRef > 0);
    --NetWsaContainer.SockRef;

    closesocket(hSock);
};


bool NetSockSetNonblock(SOCKET hSock)
{
    u_long value = TRUE;
    return (ioctlsocket(hSock, FIONBIO, &value) != SOCKET_ERROR);
};


bool NetSockSetNoDelay(SOCKET hSock, bool Flag)
{
    int32 opt = int32(Flag);
    return (setsockopt(hSock, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(int32)) != SOCKET_ERROR);
};


bool NetSockSetReuseAddr(SOCKET hSock, bool Flag)
{
    int32 opt = int32(Flag);
    return (setsockopt(hSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(int32)) != SOCKET_ERROR);
};


bool NetSockSetPortScale(SOCKET hSock, bool Flag)
{
    int32 opt = int32(Flag);
    return (setsockopt(hSock, SOL_SOCKET, SO_PORT_SCALABILITY, (const char*)&opt, sizeof(int32)) != SOCKET_ERROR);
};


bool NetSockSetBuffer(SOCKET hSock, uint32 SizeBuffRecv, uint32 SizeBuffSend)
{
    if (setsockopt(hSock, SOL_SOCKET, SO_RCVBUF, (const char*)&SizeBuffRecv, sizeof(int32)) == SOCKET_ERROR)
        return false;

    return (setsockopt(hSock, SOL_SOCKET, SO_SNDBUF, (const char*)&SizeBuffSend, sizeof(int32)) != SOCKET_ERROR);
};


bool NetSockUpdAcceptCtx(SOCKET hSockListen, SOCKET hSockAccept)
{
    int32 Result = setsockopt(
        hSockAccept,
        SOL_SOCKET,
        SO_UPDATE_ACCEPT_CONTEXT,
        (char*)&hSockListen,
        sizeof(hSockListen)
    );
    
    return (Result != SOCKET_ERROR);
};


bool NetSockUpdConnectCtx(SOCKET hSock)
{
    return (setsockopt(hSock, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0) != SOCKET_ERROR);
};


bool NetSockSetLinger(SOCKET hSock, bool Flag, uint16 Seconds)
{
    int32 Result = 0;
    
    if (Flag)
    {
        LINGER lingerOption;
        lingerOption.l_onoff = 1;
        lingerOption.l_linger = Seconds;
        Result = setsockopt(hSock, SOL_SOCKET, SO_LINGER, (char*)&lingerOption, sizeof(LINGER));
    }
    else
    {
        BOOL opt = TRUE;
        Result = setsockopt(hSock, SOL_SOCKET, SO_DONTLINGER, (char*)&opt, sizeof(BOOL));
    };

    return (Result != SOCKET_ERROR);
};


HOBJ NetSockPoolCreate(int32 Capacity)
{
    ASSERT(Capacity > 0);
    
    NetSockPool_t* NetSockPool = new NetSockPool_t();
    if (!NetSockPool)
        return 0;

    NetSockPool->Queue = new CQueue<SOCKET>(Capacity);
    if (!NetSockPool->Queue)
    {
        delete NetSockPool;
        NetSockPool = nullptr;
        return 0;
    };

    ++NetWsaContainer.SockPoolRef;

    return NetSockPool;
};


void NetSockPoolDestroy(HOBJ hSockPool)
{
    NetSockPool_t* NetSockPool = (NetSockPool_t*)hSockPool;

    ASSERT(NetWsaContainer.SockPoolRef > 0);
    --NetWsaContainer.SockPoolRef;

    if (NetSockPool->Queue)
    {
        while (!NetSockPool->Queue->IsEmpty())
        {
            SOCKET hSocket = NetSockPool->Queue->Front();
            NetSockPool->Queue->Pop();

            NetSockClose(hSocket);
        };

        delete NetSockPool->Queue;
        NetSockPool->Queue = nullptr;
    };

    delete NetSockPool;
};


SOCKET NetSockPoolAlloc(HOBJ hSockPool)
{
    NetSockPool_t* NetSockPool = (NetSockPool_t*)hSockPool;
    SOCKET hResult = INVALID_SOCKET;
    
    {
        std::lock_guard<std::recursive_mutex> Lock(NetSockPool->Mutex);
		if (!NetSockPool->Queue->IsEmpty())
		{
			hResult = NetSockPool->Queue->Front();
			NetSockPool->Queue->Pop();
		};
    }

    return hResult;
};


void NetSockPoolFree(HOBJ hSockPool, SOCKET hSock)
{
    NetSockPool_t* NetSockPool = (NetSockPool_t*)hSockPool;

    {
        std::lock_guard<std::recursive_mutex> Lock(NetSockPool->Mutex);
        ASSERT(!NetSockPool->Queue->IsFull());
        NetSockPool->Queue->Push(hSock);
    }
};


bool NetSockPoolIsFull(HOBJ hSockPool)
{
    NetSockPool_t* NetSockPool = (NetSockPool_t*)hSockPool;

    std::lock_guard<std::recursive_mutex> Lock(NetSockPool->Mutex);
    return NetSockPool->Queue->IsFull();
};


bool NetDnsResolve(const char* Hostname, uint32* AddrArray, int32* AddrArrayCount)
{
    ASSERT(Hostname);
    ASSERT(AddrArray);
    ASSERT(AddrArrayCount);
    ASSERT(*AddrArrayCount > 0);

    if ((*AddrArrayCount) <= 0)
        return false;

    {
        std::lock_guard<std::recursive_mutex> Lock(NetDns->Mutex);

        auto it = NetDns->CacheMap.find(Hostname);
        if (it != NetDns->CacheMap.end())
        {
            NetDnsResolveData_t* ResolveData = &(*it).second;

            uint32 TimeNow = TimeCurrentTick();
            if (TimeNow < (ResolveData->TimeWhenResolved + NET_DNS_CACHE_TIMEOUT))
            {
                AddrArray[0] = ResolveData->Address;
                *AddrArrayCount = 1;

                return true;
            }
            else
            {
                NetDns->CacheMap.erase(it);
            };
        };
    }

    ADDRINFOA AddrInfo = {};
    AddrInfo.ai_family = AF_INET;
    AddrInfo.ai_socktype = SOCK_STREAM;
    AddrInfo.ai_protocol = IPPROTO_TCP;

    ADDRINFO* AddrInfoResult = nullptr;
    if (!getaddrinfo(Hostname, nullptr, &AddrInfo, &AddrInfoResult))
    {
        int32 i = 0;
        ADDRINFO* AddrInfoRecord = AddrInfoResult;
        while (i < *AddrArrayCount)
        {
            AddrArray[i++] = ntohl(LPSOCKADDR_IN(AddrInfoRecord->ai_addr)->sin_addr.s_addr);

            AddrInfoRecord = AddrInfoRecord->ai_next;
            if (!AddrInfoRecord)
                break;
        };

        *AddrArrayCount = i;
        freeaddrinfo(AddrInfoResult);

        {
            std::lock_guard<std::recursive_mutex> Lock(NetDns->Mutex);

            NetDnsResolveData_t ResolveData = {};
            ResolveData.Address = AddrArray[0];
            ResolveData.TimeWhenResolved = TimeCurrentTick();

            NetDns->CacheMap.insert({ Hostname, ResolveData });
        };

        return true;
    }
    else
    {
        *AddrArrayCount = 0;
        OUTPUTLN("Resolve hostname '%s' failed %u", Hostname, WSAGetLastError());
        
        return false;
    };
};