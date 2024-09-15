#include "TestWebsocket.hpp"
#include "TestUserAgent.hpp"

#include "Utils/Websocket/Websocket.hpp"
#include "Utils/Base64/Base64.hpp"
#include "Utils/Misc/WebUtils.hpp"

#include "Shared/Common/Random.hpp"


enum TestWebsocketMode {
	/* test fully user-defined request */
	TestWebsocketMode_UserRQ = 0,

	/* test default websocket class request */
	TestWebsocketMode_DefaultRQ,

	/* test websocket extension request only */
	TestWebsocketMode_WsExt,
};


static void TestWebsocketRQ(TestWebsocketMode mode)
{
    enum teststep
    {
        /* in order of action */
        teststep_idle = 0,
        teststep_connect,
        teststep_pong,
        teststep_echo,
        teststep_dc,
        teststep_end,
    };

    teststep step = teststep_idle;
    const std::string echostr = "testpayload123";
    const std::string host = "ws.ifelse.io";
    
    CWebsocket ws;
    ws.on_connect([&](CWebsocket& ws) {
        ASSERT(step == teststep_idle);
        step = teststep_connect;
    });
    ws.on_read([&](CWebsocket& ws, const void* data, std::size_t size, bool binary) {
        switch (step)
        {
        case teststep_connect:
			ASSERT(binary == false);
            step = teststep_pong;
            ws.ping();
            break;

        case teststep_echo:
			ASSERT(binary == true);
            ASSERT(std::string(reinterpret_cast<const char*>(data), size) == echostr);
            step = teststep_dc;
            ws.close();
            break;
            
        default:
            ASSERT(false);
            break;
        };
    });
    ws.on_disconnect([&](CWebsocket& ws, CWebsocket::closecode cc) {
        ASSERT(step == teststep_dc);
        step = teststep_idle;
    });
    ws.on_pong([&](CWebsocket& ws, const void* data, std::size_t size) {
        ASSERT(step == teststep_pong);
        step = teststep_echo;
        ws.send_bin(echostr.c_str(), echostr.length());
    });
    ws.on_error([&](CWebsocket& ws, uint32 errcode) {
        ASSERT(false);
    });
    ws.set_masking(true);
    ws.set_compress(true);
    ws.resolve_redirect(true, 2);

    switch (mode)
    {
    case TestWebsocketMode_UserRQ:
		ws.connect(
			"wss://" + host,
			"GET wss://" + host + "/ HTTP/1.1\r\n"
            "Host: " + host + "\r\n"
            "User-Agent: " + TestUserAgent + "\r\n"
			"Accept: */*\r\n"
			"Upgrade: websocket\r\n"
			"Origin: https://" + host + "/\r\n"
			"Sec-WebSocket-Extensions: permessage-deflate; client_max_window_bits\r\n"
			"Sec-WebSocket-Key: " + CBase64::Encode(WebRndHexString(16)) + "\r\n" // RFC 6455, page 17, 7
			"Sec-WebSocket-Version: 13\r\n"
			"Connection: keep-alive, Upgrade\r\n"
			"\r\n"
		);
        break;
        
    case TestWebsocketMode_DefaultRQ:
		ws.connect("wss://" + host);
        break;
        
    case TestWebsocketMode_WsExt:
        /**
         *  Request per message deflate extension with random bits window value (8..15) and set websocket compression to true
         *  If server dont support that websocket class should automatically correct it internally depending on server response
         */
        ws.set_compress(true);
        ws.connect_ex(
            "wss://" + host,
            "permessage-deflate; client_max_window_bits=" + std::to_string(RndInt32(8, 15))
        );
        break;
        
    default:
		break;
	};
	
    ws.wait();
    ASSERT(step == teststep_idle);
};


void TestWebsocket()
{
	TestWebsocketRQ(TestWebsocketMode_UserRQ);
	TestWebsocketRQ(TestWebsocketMode_DefaultRQ);
	TestWebsocketRQ(TestWebsocketMode_WsExt);
};