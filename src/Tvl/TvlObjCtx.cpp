#include "TvlObjCtx.hpp"
#include "TvlUtil.hpp"
#include "TvlSettings.hpp"
#include "TvlResult.hpp"

#include "Utils/Misc/WebUtils.hpp"
#include "Utils/Json/Json.hpp"
#include "Utils/Base64/Base64.hpp"

#include "Shared/Network/Net.hpp"
#include "Shared/Common/UserAgent.hpp"
#include "Shared/Common/Time.hpp"
#include "Shared/Common/Random.hpp"


namespace TVLCTX
{
	const std::chrono::milliseconds HTTP_TIMEOUT(5000);
	const std::chrono::milliseconds WS_TIMEOUT(60000);

	enum STATE
	{
		STATE_WS_TICKET = 0,
		STATE_WS_TOKEN,
		STATE_WS,
	};

	enum SVCFLAG
	{
		SVCFLAG_SEQ_NEXT    = (1 << 0),
		SVCFLAG_SEQ_RST     = (1 << 1),
		SVCFLAG_STOP       	= (1 << 2),
	};

	namespace DB
	{
		const char* USERAGENT   = "db_useragent";
		const char* KEPLER      = "db_kepler";
		const char* TOKEN       = "db_token";
		const char* TICKET      = "db_ticket";
		const char* PVID        = "db_pvid";
		const char* TID         = "db_tid";
		const char* QID         = "db_qid";
	};

	enum WSOP
	{
		WSOP_UNKOWN                 = 0,
		WSOP_SUBSCRIBE              = 1,
		WSOP_SUBSCRIBE_ACK          = 2,
		WSOP_RAWMSG                 = 3,
		WSOP_RAWMSG_ACK             = 4,
		WSOP_SEND_MSG               = 5,
		WSOP_SEND_MSG_ACK           = 6,
		WSOP_HEART_BEAT             = 7,
		WSOP_HEART_BEAT_ACK         = 8,
		WSOP_CANCEL_SUBSCRIBE       = 9,
		WSOP_CANCEL_SUBSCRIBE_ACK   = 10,
		WSOP_SET_FILTER             = 11,
		WSOP_SET_FILTER_ACK         = 12,
		WSOP_SET_OPTIONS            = 13,
		WSOP_SET_OPTIONS_ACK        = 14,
		WSOP_FORCE_KICK_OFF         = 254,
		WSOP_TEST                   = 255,
	};

	enum WSMSG
	{
		WSMSG_DEFAULT               = 0,
		WSMSG_BULLET_CHAT           = 1,
	};

	enum WATCHSTATUS
	{
		WATCHSTATUS_DEFAULT         = 0,
		WATCHSTATUS_WATCHING        = 1,
		WATCHSTATUS_NOT_WATCHING    = 2,
	};

#pragma pack(push, 1)    
	struct PACKET_HDR
	{
		int32 MsgSize;
		int16 HdrSize;
		int16 Unk0;     // always 1
		int16 Opcode;
		int32 SequenceNo;
		int16 Unk1;     // always 0
		int16 Unk2;     // always 0
	};
#pragma pack(pop)

	static_assert(sizeof(PACKET_HDR) == 18, "update me");
};


//
//  Tvl ws ticket state
//
void CTvlStateWsTicket::Attach()
{
	const char* Payload = "{\"app_info\":{\"version_code\":0,\"version_name\":\"9.9.1\",\"platform\":4,\"terminal_type\":2,\"pvid\":\"%s\",\"language\":\"en-US\",\"client_type\":4,\"platformLanguage\":\"ru-RU\"},\"client_info\":{\"device_info\":\"{\\\"tid\\\":\\\"%s\\\"}\"}}";

	char PayloadFormatted[4096];
	PayloadFormatted[0] = '\0';

	std::string Trash = "{\"data\":\"B3SdDSQV3p-dnbr1p4CjSiDD6WRGY-ssTmtv9mMfSkBpL4FiDiJC8PeDw3NYnWUwmG2Ynp2dhXSYbZienZ2ZQwfy4AdQkyL3s4WV6NAmVHJmgXk5H_-tivGaJztAq0eTGSpMzz29gVOzwlZ_-AdjN1DzMwSQGce_NNC7llnTMlY9_uQh4zrd3ilhQLMi_60HlnsfJP099EWBCzrCKERQqC5IqHADVI3dW3wtsaELXPF8b5CKkwr0fMFzy4saVK3GUMFWSB0fU74uFe_HDln5DXgpuxRUfSzx4VZaeL6Ihh5I_UmprLCyEnSu_7H7AuL2woXhYzDg_LF_EYUWON1Vr8eFyg4Q_ivBc_uDQv8ohzP7C1D9e2xx6wYIdgfrWv24Mio5yJMkDp6diZ2dnZ3NyDcsGIadhcBjKyLcdq8BwAueHRXyKKxQC74G-q0B5NsbDP5576XRA3xVL2zlixPUffkpxfD7Dkztq8cxqy5U9b2rhrMbBgTyLAFRwYYG_CUspLsWXPcsLeWBPhRaj6yEUJg-UPUqB3HLFkT9qkdfbGCjLlY9OeZxl5ydZWKcnZ2d3fWpxfPBg35VLAGw2wMY7qnGs8pbDOykh_Pxg1K__STBswpWXPUrBDH7XlDxtyrHBaFTCFCwRqWB80xfKMe2wRZUx38H5AtWWPcvxaV6_yJa_KvNBhEyfBXZO4iiyqo0--7p577HBlgprgsod1ByPyC0oU4uxXBh4C4z92uBr8IbdkRJ5DC74CgYePjdnZ2d3Z2dnZ2dnZ2dnZ0*\"}";

	int32 Len = sprintf_s(
		PayloadFormatted,
		sizeof(PayloadFormatted),
		Payload,
		datastore().get(TVLCTX::DB::PVID).c_str(),
		datastore().get(TVLCTX::DB::TID).c_str()
	);

	request().set_read_timeout(TVLCTX::HTTP_TIMEOUT);
	request().on_error([ & ](CHttpReq& req, uint32 errcode) { Subject().ServiceRequest(TVLCTX::SVCFLAG_SEQ_RST); });
	request().on_complete([ & ](CHttpReq& req, CHttpResponse& resp) {
		bool bResult = false;

		if (resp.status() == httpstatus::code_ok)
		{
			try
			{
				CJson json(resp.body(), resp.body_size());
				ASSERT(datastore().get_count(TVLCTX::DB::TICKET) == 0);
				datastore().set(TVLCTX::DB::TICKET, json["body"]["ticket"].data());
				bResult = true;
			}
			catch (CJson::exception& e)
			{
				(void)e;
				OUTPUTLN("json --> %s", e.what());
			};
		};

		Subject().ServiceRequest(bResult ? TVLCTX::SVCFLAG_SEQ_NEXT : TVLCTX::SVCFLAG_SEQ_RST);
		return false;
	});
	request().send(
		"https://api.trovo.live",
		"POST /cgi/rpc/security/WebTicketService/GetTicket?crf=5381&comm=" + CBase64::Encode(PayloadFormatted) + " HTTP/1.1\r\n"
		"Host: api.trovo.live\r\n"
		"Accept: */*\r\n"
		"Referer: https://trovo.live/\r\n"
		"Content-Type: text/plain;charset=UTF-8\r\n"
		"User-Agent: " + datastore().get(TVLCTX::DB::USERAGENT) + "\r\n"
		"Content-Length: " + std::to_string(Trash.length()) + "\r\n"
		"Cookie: pgg_pvid=" + datastore().get(TVLCTX::DB::PVID) + "\r\n"
		"Connection: close\r\n"
		"\r\n"
		+ Trash,
		TVLCTX::HTTP_TIMEOUT
	);
};


//
//  Tvl ws token state
//
void CTvlStateWsToken::Attach()
{
	const char* QueryFormat = "chunk=1&reqms=%s&qid=%s&cli=4&client_info={%%22device_info%%22:%%22{\\%%22tid\\%%22:\\%%22%s\\%%22}%%22}&from=%%2Fs%%2F%s&locale=RU&resolution=824*1536";
	const char* PayloadFormat = "[{\"operationName\":\"ws_WSTokenService_GetToken\",\"variables\":{\"params\":{\"subinfo\":{\"page\":{\"scene\":\"SCENE_CHANNEL\",\"pageID\":%s},\"streamerID\":%s}}},\"extensions\":{\"webTicket\":\"%s\",\"singleReq\":true}}]";

	char Query[4096];
	Query[0] = '\0';

	char Payload[4096];
	Payload[0] = '\0';

	int32 QueryLen = sprintf_s(
		Query,
		sizeof(Query),
		QueryFormat,
		std::to_string(TimeCurrentUnix64()).c_str(),
		TvluGenerateQID().c_str(),
		TvluGenerateTID().c_str(),
		TvlSettings.TargetId
	);

	int32 PayloadLen = sprintf_s(
		Payload,
		sizeof(Payload),
		PayloadFormat,
		std::to_string(CTvlObjCtx::TargetChannelId).c_str(),
		std::to_string(CTvlObjCtx::TargetChannelId).c_str(),
		datastore().get(TVLCTX::DB::TICKET).c_str()
	);
	
	request().set_read_timeout(TVLCTX::HTTP_TIMEOUT);
	request().on_error([ & ](CHttpReq& req, uint32 errcode) { Subject().ServiceRequest(TVLCTX::SVCFLAG_SEQ_RST); });
	request().on_complete([ & ](CHttpReq& req, CHttpResponse& resp) {
		bool bResult = false;

		if (resp.status() == httpstatus::code_ok)
		{
			try
			{
				CJson json(resp.body(), resp.body_size());
				datastore().set(TVLCTX::DB::TOKEN, json[0]["data"]["ws_WSTokenService_GetToken"]["token"].data());
				bResult = true;
			}
			catch (CJson::exception& e)
			{
				(void)e;
				OUTPUTLN("json --> %s", e.what());
			};
		};

		Subject().ServiceRequest(bResult ? TVLCTX::SVCFLAG_SEQ_NEXT : TVLCTX::SVCFLAG_SEQ_RST);
		return false;
	});
	request().send(
		"https://api.trovo.live",
		"POST /graphql?" + std::string(Query) + " HTTP/1.1\r\n"
		"Host: api.trovo.live\r\n"
		"Referer: https://trovo.live/\r\n"
		"User-Agent: " + datastore().get(TVLCTX::DB::USERAGENT) + "\r\n"
		"Accept: */*\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: " + std::to_string(PayloadLen) + "\r\n"
		"Cookie: pgg_pvid=" + datastore().get(TVLCTX::DB::PVID) + "\r\n"
		"Connection: close\r\n"
		"\r\n"
		+ Payload,
		TVLCTX::HTTP_TIMEOUT
	);
};


//
//  Tvl ws chat state
//
void CTvlStateWs::Attach()
{
	m_bBotFlag = false;
	m_PingTimer.SetAlarm(20000 + RndUInt32(2500, 7500));
	m_PingTimer.Reset();

	websocket().on_error([&](CWebsocket& ws, int32 errcode) { Subject().ServiceRequest(TVLCTX::SVCFLAG_SEQ_RST); });
	websocket().on_connect([&](CWebsocket& ws) {
		ws.set_timeout(TVLCTX::WS_TIMEOUT);

		std::string Token = datastore().get(TVLCTX::DB::TOKEN);
		std::vector<unsigned char> Input;

		//
		//  Packet header + payload header + payload
		//
		Input.resize(sizeof(TVLCTX::PACKET_HDR) + 3 + Token.size());

		TVLCTX::PACKET_HDR* Hdr = (TVLCTX::PACKET_HDR*)&Input[0];
		Hdr->MsgSize 	= byteswap4(Input.size());
		Hdr->HdrSize 	= byteswap2(sizeof(TVLCTX::PACKET_HDR));
		Hdr->Unk0 		= byteswap2(1);
		Hdr->Opcode 	= byteswap2(TVLCTX::WSOP_SUBSCRIBE);
		Hdr->SequenceNo = byteswap4(1);

		*(uint8*)&Input[sizeof(TVLCTX::PACKET_HDR) + 0] = uint8(0x0A);
		*(uint8*)&Input[sizeof(TVLCTX::PACKET_HDR) + 1] = uint8((127 & Token.size()) | 128);
		*(uint8*)&Input[sizeof(TVLCTX::PACKET_HDR) + 2] = uint8(Token.size() >> 7);

		std::memcpy(&Input[sizeof(TVLCTX::PACKET_HDR) + 3], &Token[0], Token.size());

		ws.send_bin(&Input[0], Input.size());
	});
	websocket().on_read([&](CWebsocket& ws, const void* data, std::size_t size, bool binary) {
		if (!binary)
		{
			OUTPUTLN("unexpected data format");
			ws.close();
			return;
		};

		if (size < sizeof(TVLCTX::PACKET_HDR))
		{
			OUTPUTLN("unexpected packet size");
			ws.close();
			return;
		};

		const TVLCTX::PACKET_HDR* Hdr = reinterpret_cast<const TVLCTX::PACKET_HDR*>(data);
		TVLCTX::WSOP wsop = TVLCTX::WSOP(byteswap2(Hdr->Opcode));

		switch (wsop)
		{
		case TVLCTX::WSOP_SUBSCRIBE_ACK:
			SendPing();
			break;

		case TVLCTX::WSOP_HEART_BEAT_ACK:
			switch (TvlSettings.RunMode)
			{
			case TVLRUNMODE_KEEPALIVE:
				if (!m_bBotFlag)
				{
					m_bBotFlag = true;
					TvlResult.AddViewerCount(1);
				};				
				break;

			case TVLRUNMODE_RST:
				{
					ws.abort();
					Subject().ServiceRequest(TVLCTX::SVCFLAG_SEQ_RST);
				}
				break;

			default:
				ASSERT(false);
				break;
			};
			break;

		default:
			break;
		};
	});
	websocket().on_disconnect([&](CWebsocket& ws, CWebsocket::closecode closecode) {
		if (m_bBotFlag)
			TvlResult.AddViewerCount(-1);

		if (closecode != CWebsocket::closecode_normal)
			OUTPUTLN("unexpected closecode %d - %s", closecode, CWebsocket::closecode_to_str(closecode));
		
		Subject().ServiceRequest(TVLCTX::SVCFLAG_SEQ_RST);
	});
	websocket().set_compress(false);
	websocket().set_masking(true);
	websocket().set_timeout(TVLCTX::HTTP_TIMEOUT);
	websocket().connect(
		"wss://chat.trovo.live",
		"GET /sub HTTP/1.1\r\n"
		"Host: chat.trovo.live\r\n"
		"User-Agent: " + datastore().get(TVLCTX::DB::USERAGENT) + "\r\n"
		"Accept: */*\r\n"
		"Origin: https://trovo.live\r\n"	// RFC 6455, page 18, 8
		"Upgrade: websocket\r\n"
		"Sec-WebSocket-Extensions: permessage-deflate; client_max_window_bits\r\n"
		"Sec-WebSocket-Key: " + CBase64::Encode(WebRndHexString(16)) + "\r\n" // RFC 6455, page 17, 7
		"Sec-WebSocket-Version: 13\r\n"
		"Connection: keep-alive, Upgrade\r\n"
		"\r\n",
		TVLCTX::HTTP_TIMEOUT
	);
};


void CTvlStateWs::Detach(void)
{
	;
};


void CTvlStateWs::Observing(void)
{
	m_PingTimer.Period();

	if (m_PingTimer.IsAlarm())
	{
		m_PingTimer.Reset();
		SendPing();
	};
	
	CTvlStateBase::Observing();
};


void CTvlStateWs::SendPing(void)
{
	std::vector<unsigned char> Input;

	//
	//  Packet header + heartbeat header + heartbeat payload
	//
	Input.resize(sizeof(TVLCTX::PACKET_HDR) + 2);

	TVLCTX::PACKET_HDR* Hdr = (TVLCTX::PACKET_HDR*)&Input[0];
	Hdr->MsgSize    = byteswap4(Input.size());
	Hdr->HdrSize    = byteswap2(sizeof(TVLCTX::PACKET_HDR));
	Hdr->Unk0       = byteswap2(1);
	Hdr->Opcode     = byteswap2(TVLCTX::WSOP_HEART_BEAT);
	Hdr->SequenceNo = 0;    // always 0 in heartbeat packet type

	*(uint8*)&Input[sizeof(TVLCTX::PACKET_HDR) + 0] = uint8(0x10);
	*(uint8*)&Input[sizeof(TVLCTX::PACKET_HDR) + 1] = uint8(TVLCTX::WATCHSTATUS_WATCHING);

	websocket().send_bin(&Input[0], Input.size());
};


//
//  Tvl ctx obj
//
/*static*/ int32 CTvlObjCtx::TargetChannelId = -1;


void CTvlObjCtx::Start()
{
	/* init data store */
	SetInitialDatastore();

	/* init shared */
	SetShared(&m_shared);

	/* init state & seq */
	StateRegist(TVLCTX::STATE_WS_TICKET, &m_stateWsTicket);
	StateRegist(TVLCTX::STATE_WS_TOKEN, &m_stateWsToken);
	StateRegist(TVLCTX::STATE_WS, &m_stateWs);
	m_seq = CProxyObjSeq(TVLCTX::STATE_WS_TICKET, TVLCTX::STATE_WS);

	/* request seq reset */
	ServiceRequest(TVLCTX::SVCFLAG_SEQ_RST);
};


void CTvlObjCtx::Stop()
{
	m_shared.websocket.abort();
};


void CTvlObjCtx::Service(uint32 uServiceFlags)
{
	if (uServiceFlags & TVLCTX::SVCFLAG_STOP)
	{
		StateJump(STATE_STOP);
		return;
	};
	
	if (uServiceFlags & TVLCTX::SVCFLAG_SEQ_RST)
	{
		ASSERT((uServiceFlags & TVLCTX::SVCFLAG_SEQ_NEXT) == 0);
		uServiceFlags &= ~(TVLCTX::SVCFLAG_SEQ_NEXT);

		SetInitialDatastore();
		m_seq.Reset();
		StateJump(m_seq.Current());
	};

	if (uServiceFlags & TVLCTX::SVCFLAG_SEQ_NEXT)
	{
		m_seq.StrideNext();
		StateJump(m_seq.Current());
	};
};


void CTvlObjCtx::SetInitialDatastore()
{
	m_shared.datastore.clear();
	m_shared.datastore.set(TVLCTX::DB::USERAGENT, UserAgentGenereate());
	m_shared.datastore.set(TVLCTX::DB::PVID, TvluGeneratePVID());
	m_shared.datastore.set(TVLCTX::DB::TID, TvluGenerateTID());
};