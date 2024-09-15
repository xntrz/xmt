#include "TestNet.hpp"

#include "Shared/Common/Event.hpp"
#include "Shared/Common/Thread.hpp"
#include "Shared/Common/Random.hpp"
#include "Shared/Network/Net.hpp"

#include "Utils/Http/HttpResponse.hpp"


static bool s_bSSL = false;


static std::vector<std::string> GenerateRandomStringVariations(std::size_t lenPerVariation, std::size_t cntVariation)
{
	std::vector<std::string> result;

	for (std::size_t i = 0; i < cntVariation; ++i) {
		/* generate rand string len at step */
		std::size_t lenMin = (lenPerVariation * i);
		std::size_t lenMax = (lenPerVariation * (i + 1));
		std::size_t len = RndUInt32(lenMin, lenMax);

		/* fill string with random chars */
		std::string data;
		data.resize(len);

		std::for_each(data.begin(), data.end(), [](char& ch) {
			ch = char(RndUInt32(0x61, 0x7A)); // a-z
		});

		result.push_back(std::move(data));
	};

	return result;
};


static void TestAsyncEchoClientServer(std::size_t passCount, std::size_t clientCount, bool fDOC = false, bool fLargeData = false)
{
    CEvent evtEnd;
	std::vector<std::string> testdata = GenerateRandomStringVariations(fLargeData ? 4096u : 1024u, fLargeData ? 8u : 1u);

    /* start server */
    HCONN hSv = NetTcpOpen([&](HCONN hConn, NETEVENT evt, uint32 err, uint64 addr, const void* data, uint32 size, void* param) {
        switch (evt) {
        case NETEVENT_CONNECT:
            break;
        case NETEVENT_RECV:
			NetTcpSend(hConn, data, size, fDOC);			
            break;
        case NETEVENT_DISCONNECT:
            NetTcpClose(hConn);
            break;
        default:
            ASSERT(false);
            break;
        };
        return true;
    });
    NetTcpSetSecure(hSv, s_bSSL);
    NetTcpListenSelf(hSv, int32(clientCount), 8886);

    /* start clients */
    std::atomic<std::size_t> clientsExit = 0;
    
    std::vector<HCONN> clients;
    clients.reserve(clientCount);
    
    std::vector<std::size_t> clientsPasses;
    clientsPasses.resize(clientCount);

    struct client_ctx {
        std::size_t idx;
        std::size_t bytes;
    };

    std::vector<client_ctx> clientCtx;
    clientCtx.resize(clientCount);

    for (std::size_t i = 0; i < clientCount; ++i) {
        HCONN hCl = NetTcpOpen([&, i, &ctx = clientCtx[i]](HCONN hConn, NETEVENT evt, uint32 err, uint64 addr, const void* data, uint32 size, void* param) {
            switch (evt) {
            case NETEVENT_CONNECTFAIL:
            case NETEVENT_CONNECTFAILSSL:
                ASSERT(false);
                break;
            case NETEVENT_CONNECT:
                {
                    ctx.idx = (fLargeData ? (RndUInt32() % testdata.size()) : 0);
                    const std::string& d = testdata[ctx.idx];
                    NetTcpSend(hConn, &d[0], d.length());
                }
                break;
            case NETEVENT_RECV:
                {
                    std::string recvStr(reinterpret_cast<const char*>(data), size);
                    std::string partStr(&testdata[ctx.idx][0] + ctx.bytes, std::min(testdata[ctx.idx].length(), size_t(size)));
                    ASSERT(recvStr == partStr);

                    ctx.bytes += size;
                    ASSERT(ctx.bytes <= testdata[ctx.idx].length());
                    
                    if (ctx.bytes == testdata[ctx.idx].length())
                        NetTcpDisconnect(hConn);
                }
                break;
            case NETEVENT_DISCONNECT:
                if (++clientsPasses[i] >= passCount) {
                    ++clientsExit;
                    evtEnd.signal_once();
                }
                else {
                    if (!NetTcpConnectSelf(hConn, 8886))
                        ASSERT(false);
                };
                break;
            default:
                ASSERT(false);
                break;
            };
            return true;
        });
        clients.push_back(hCl);
		NetTcpSetSecure(hCl, s_bSSL);
		NetTcpSetForceClose(hCl, true);
        NetTcpConnectSelf(hCl, 8886);
    };
    
	evtEnd.wait([&]() {
		return clientsExit == clients.size(); 
	});

    std::for_each(clients.begin(), clients.end(), [](HCONN hConn) {
        NetTcpClose(hConn);
	});

    NetTcpClose(hSv);
};


static void TestAsyncRstClientServer()
{
    std::atomic<std::size_t> okCount = 0;
    CEvent evtEnd;

    HCONN hSv = NetTcpOpen([&](HCONN hConn, NETEVENT evt, uint32 err, uint64 addr, const void* data, uint32 size, void* param) {
        switch (evt) {
        case NETEVENT_CONNECT:
            ++okCount;
            NetTcpClose(hConn); /* close user reference */
            return false;		/* reject connection */
        default:
            ASSERT(false);
            break;
        };
        return true;
    });
    NetTcpSetSecure(hSv, s_bSSL);
    NetTcpListenSelf(hSv, 1, 8886);

    HCONN hCl = NetTcpOpen([&](HCONN hConn, NETEVENT evt, uint32 err, uint64 addr, const void* data, uint32 size, void* param) {
        switch (evt) {
        case NETEVENT_CONNECT:
            break;
        case NETEVENT_DISCONNECT:
            ASSERT(okCount == 1);
            evtEnd.signal_all();
            break;
        default:
            ASSERT(false);
            break;
        };
        return true;
    });
    NetTcpSetSecure(hCl, s_bSSL);
    NetTcpConnectSelf(hCl, 8886);
    
    evtEnd.wait();
    NetTcpClose(hSv);
    NetTcpClose(hCl);
};


static void TestAsyncCancelation()
{
	CEvent evtEnd;

	std::size_t okCount = 0;

	HCONN hConn = NetTcpOpen([&](HCONN hConn, NETEVENT evt, uint32 err, uint64 addr, const void* data, uint32 size, void* param) {
		switch (evt) {
		case NETEVENT_CONNECTFAIL:
			ASSERT(err == 995);
			++okCount;
			evtEnd.signal_all();
			break;
		default:
			ASSERT(false);
			break;
		};
		return true;
	});
	if (!NetTcpConnectSelf(hConn, 8886))
		ASSERT(false);
	if (!NetTcpCancelConnect(hConn))
		ASSERT(false);
	evtEnd.wait();
	ASSERT(okCount == 1);
	NetTcpClose(hConn);
};


static void TestSyncClient()
{
    std::string request =
        "GET / HTTP/1.1\r\n"
        "Host: www.google.com\r\n"
        "Connection: close\r\n"
        "\r\n";

	CHttpResponse response;

    /* do not specify "EventProc" & "Param" to enable sync mode */
    HCONN hConn = NetTcpOpen();
    ASSERT(hConn);
    
    if (hConn) {
        /* enable SSL mode */
        NetTcpSetSecure(hConn, true);
        
        /* set hint for SSL */
        NetTcpSetSecureHost(hConn, "www.google.com");
        
        /* google at this moment have 10 seconds timeout for reads then dc us */
        NetTcpSetTimeoutRead(hConn, 5000);
        
        if (NetTcpConnect(hConn, "https://www.google.com")) {
            /* should be non zero for sync writes on success*/
            std::size_t written = NetTcpSend(hConn, &request[0], request.length());
            ASSERT(written == request.length());

            /* read all data by 1024 bytes chunk until eof (0 bytes) */
            char buffer[1024];
            std::size_t bytes = 0;
            while (bytes = NetTcpRecv(hConn, buffer, sizeof(buffer))) {
                if (!response.process(buffer, bytes))
                    ASSERT(false, "something gone wrong when processing http data");
                if (response.is_complete()) {
                    ASSERT(response.is_keepalive() == false);
                    ASSERT(response.status() == httpstatus::code_ok);
                };
            };

            /* we got dc while read as eof because of "Connection: close" so dc request should fail because we not connected */
            bool bDcRet = NetTcpDisconnect(hConn);
            ASSERT(bDcRet == false);
        }
        else {
            ASSERT(false, "couldn't connect to https://www.google.com");
        };
        
        NetTcpClose(hConn);
    };
};



static void TestSyncConnectFail()
{
    HCONN hCl = NetTcpOpen();
	bool bSelfConnRet = NetTcpConnectSelf(hCl, 8886);
    ASSERT(bSelfConnRet == false);
    NetTcpClose(hCl);
};


static void TestAsyncClientProxyHTTP()
{

};


static void TestAsyncClientProxySOCKS4()
{

};


static void TestAsyncClientProxySOCKS4A()
{

};


static void TestAsyncClientProxySOCKS5()
{

};


static void TestAsyncResolve(std::size_t numSimultaneousResolves)
{
    static const char* hosts[] = {
        "localhost",
        "www.google.com",
        "www.yandex.ru",
        "www.youtube.com",
    };
    
    CEvent evtCompletion;
    std::atomic<std::size_t> cntCompletion = 0;
    
    HCONN hConn = NetTcpOpen([&](HCONN hConn, NETEVENT evt, uint32 err, uint64 addr, const void* data, uint32 size, void* param) {
        switch (evt) {
        case NETEVENT_RESOLVE:
            ASSERT(err == 0);
            ASSERT(size > 0, "no ips found");
            ++cntCompletion;
            evtCompletion.signal_once();
            break;
        default:
            ASSERT(false);
            break;
        };
        return true;
    });

    for (std::size_t i = 0; i < numSimultaneousResolves; ++i) {
        /* NOTE: async request ignores net subsystem resolver cache */
        NetTcpResolve(hConn, hosts[i % COUNT_OF(hosts)]);
    };

    evtCompletion.wait([&]() {
        return (cntCompletion == numSimultaneousResolves);
    });
    
    NetTcpClose(hConn);
};


void TestNet()
{
     /* do 2 passes first for without SSL and second with SSL */
    std::size_t passCount = 1;
	for (std::size_t i = 0; i < passCount; ++i)
	{
		s_bSSL = (i > 0);
        TestAsyncEchoClientServer(1, 1, false, false);
		TestAsyncEchoClientServer(1, 1, false, true);
        TestAsyncEchoClientServer(1, 5000, true, false);
        TestAsyncEchoClientServer(1, 5000, true, true);
        TestAsyncRstClientServer();
        TestSyncConnectFail();
        TestSyncClient();
        TestAsyncCancelation();        

        /**
         *  TODO find stable proxies for tests
         */
        TestAsyncClientProxyHTTP();
        TestAsyncClientProxySOCKS4();
        TestAsyncClientProxySOCKS4A();
        TestAsyncClientProxySOCKS5();
    };

    TestAsyncResolve(1);
    TestAsyncResolve(4);
    TestAsyncResolve(8);
};