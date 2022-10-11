#include "NetTest.hpp"

#include "Shared/Network/Net.hpp"
#include "Shared/Common/Event.hpp"
#include "Shared/Common/Random.hpp"


struct TestCtx_t
{
    std::atomic<int32> Alive;
    HOBJ hSyncEvent;
};


static TestCtx_t TestCtx;


static bool SvProc(
    HOBJ        hConn,
    NetEvent_t  Event,
    uint32      ErrorCode,
    uint64      NetAddr,
    const char* Data,
    uint32      DataSize,
    void*       Param
)
{
    switch (Event)
    {
    case NetEvent_Connect:
        {
            NetTcpSetTimeoutRead(hConn, 10000);
        }
        break;

    case NetEvent_ConnectFail:
    case NetEvent_ConnectFailSsl:
        {
            NetTcpClose(hConn);
        }
        break;

    case NetEvent_Recv:
        {
            NetTcpDisconnect(hConn);
        }
        break;

    case NetEvent_Disconnect:
        {
            NetTcpClose(hConn);
        }
        break;
    };

    return true;
};


static bool ClProc(
    HOBJ        hConn,
    NetEvent_t  Event,
    uint32      ErrorCode,
    uint64      NetAddr,
    const char* Data,
    uint32      DataSize,
    void*       Param
)
{
    switch (Event)
    {
    case NetEvent_Connect:
        {
            char Data[4096];
            std::for_each(
                std::begin(Data),
                std::end(Data),
                [&](char& ch) { ch = RndInt32(0, 255); }
            );
            NetTcpSend(hConn, Data, COUNT_OF(Data));
        }
        break;

    case NetEvent_ConnectFail:
    case NetEvent_ConnectFailSsl:
        {
            NetTcpClose(hConn);

            ASSERT(TestCtx.Alive > 0);
            if (!--TestCtx.Alive)
                EventSignalOnce(TestCtx.hSyncEvent);
        }
        break;

    case NetEvent_Recv:
        {
            ;
        }
        break;

    case NetEvent_Disconnect:
        {
            NetTcpClose(hConn);

            ASSERT(TestCtx.Alive > 0);
            if (!--TestCtx.Alive)
                EventSignalOnce(TestCtx.hSyncEvent);
        }
        break;
    };

    return true;
};


static void NetTestStart(bool TestSsl)
{
    static const int32 Cnt = 2048;
    static const uint16 Port = 8886;
    
    HOBJ hSv = NetTcpOpen(SvProc);
    ASSERT(hSv);
    NetTcpSetSecure(hSv, TestSsl);
    if (NetTcpListenSelf(hSv, Port))
    {
        TestCtx.hSyncEvent = EventCreate();
        TestCtx.Alive = Cnt;

        for (int32 i = 0; i < Cnt; ++i)
        {
            HOBJ hCl = NetTcpOpen(ClProc, &TestCtx);
            ASSERT(hCl);
            NetTcpSetSecure(hCl, TestSsl);
            NetTcpConnectSelf(hCl, Port);
            NetTcpCopy(hCl);
            NetTcpClose(hCl);
        };

        EventWait(TestCtx.hSyncEvent, [](void* Param) { return TestCtx.Alive == 0; });
        EventDestroy(TestCtx.hSyncEvent);
        NetTcpClose(hSv);
    };

	TestCtx.hSyncEvent = 0;
	TestCtx.Alive = 0;
};


void NetTest(void)
{
    NetTestStart(false);
    NetTestStart(true);
};