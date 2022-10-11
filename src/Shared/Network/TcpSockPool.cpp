#include "TcpSockPool.hpp"
#include "NetSockUtils.hpp"
#include "NetSettings.hpp"


struct TcpSockPool_t
{
    HOBJ hSockPoolBound;
    HOBJ hSockPoolUnbound;
    int32 Capacity;
};


static TcpSockPool_t TcpSockPool;


void TcpSockPoolInitialize(void)
{
    TcpSockPool.Capacity = NetSettings.TcpSockPoolSize;
    if (TcpSockPool.Capacity)
    {
        TcpSockPool.hSockPoolBound = NetSockPoolCreate(TcpSockPool.Capacity);
        TcpSockPool.hSockPoolUnbound = NetSockPoolCreate(TcpSockPool.Capacity);
        ASSERT(TcpSockPool.hSockPoolBound != 0);
        ASSERT(TcpSockPool.hSockPoolUnbound != 0);
    };
};


void TcpSockPoolTerminate(void)
{
    if (TcpSockPool.Capacity)
    {
        if (TcpSockPool.hSockPoolUnbound)
        {
            NetSockPoolDestroy(TcpSockPool.hSockPoolUnbound);
            TcpSockPool.hSockPoolUnbound = 0;
        };

        if (TcpSockPool.hSockPoolBound)
        {
            NetSockPoolDestroy(TcpSockPool.hSockPoolBound);
            TcpSockPool.hSockPoolBound = 0;
        };

        TcpSockPool.Capacity = 0;
    };    
};


SOCKET TcpSockPoolAllocBound(void)
{
    if (TcpSockPool.Capacity == 0)
        return NetSockNewConnect();
    
    SOCKET hSock = NetSockPoolAlloc(TcpSockPool.hSockPoolBound);
    if (hSock == INVALID_SOCKET)
    {
        for (int32 i = 0; i < TcpSockPool.Capacity; ++i)
        {
            hSock = NetSockNewConnect();
            ASSERT(hSock != INVALID_SOCKET);
            TcpSockPoolFreeBound(hSock);
        };

        hSock = TcpSockPoolAllocBound();
    };

    return hSock;
};


SOCKET TcpSockPoolAllocUnbound(void)
{
    if (TcpSockPool.Capacity == 0)
        return NetSockNewAccept();
    
    SOCKET hSock = NetSockPoolAlloc(TcpSockPool.hSockPoolUnbound);
    if (hSock == INVALID_SOCKET)
    {
        for (int32 i = 0; i < TcpSockPool.Capacity; ++i)
        {
            hSock = NetSockNewAccept();
            ASSERT(hSock != INVALID_SOCKET);
            TcpSockPoolFreeUnbound(hSock);
        };

        hSock = TcpSockPoolAllocUnbound();
    };

    return hSock;
};


void TcpSockPoolFreeBound(SOCKET hSock)
{
    if (TcpSockPool.Capacity == 0)
    {
        NetSockClose(hSock);
        return;
    };

    if (NetSockPoolIsFull(TcpSockPool.hSockPoolBound))
        NetSockClose(hSock);
    else
        NetSockPoolFree(TcpSockPool.hSockPoolBound, hSock);
};


void TcpSockPoolFreeUnbound(SOCKET hSock)
{
    if (TcpSockPool.Capacity == 0)
    {
        NetSockClose(hSock);
        return;
    };
    
    if (NetSockPoolIsFull(TcpSockPool.hSockPoolUnbound))
        NetSockClose(hSock);
    else
        NetSockPoolFree(TcpSockPool.hSockPoolUnbound, hSock);
};