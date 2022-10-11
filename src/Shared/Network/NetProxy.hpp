#pragma once

#define NETPROXY_NAME_MAX 							256
#define NETPROXY_BUFF_MAX 							2048

#define NETPROXY_SOCKS4_VERSION 					0x04
#define NETPROXY_SOCKS4_CMD_CONNECT 				0x01
#define NETPROXY_SOCKS4_CMD_BIND 					0x02
#define NETPROXY_SOCKS4_STATUS_GRANTED 				0x5A
#define NETPROXY_SOCKS4_STATUS_REJECTED 			0x5B
#define NETPROXY_SOCKS4_STATUS_IDENTD_NOT_RUNNING 	0x5C
#define NETPROXY_SOCKS4_STATUS_CONFIRM_USERID 		0x5D

#define NETPROXY_SOCKS4A_VERSION 					(NETPROXY_SOCKS4_VERSION)
#define NETPROXY_SOCKS4A_CMD_CONNECT 				(NETPROXY_SOCKS4_CMD_CONNECT)
#define NETPROXY_SOCKS4A_CMD_BIND 					(NETPROXY_SOCKS4_CMD_BIND)
#define NETPROXY_SOCKS4A_STATUS_GRANTED 			(NETPROXY_SOCKS4_STATUS_GRANTED)
#define NETPROXY_SOCKS4A_STATUS_REJECTED 			(NETPROXY_SOCKS4_STATUS_REJECTED)
#define NETPROXY_SOCKS4A_STATUS_IDENTD_NOT_RUNNING 	(NETPROXY_SOCKS4_STATUS_IDENTD_NOT_RUNNING)
#define NETPROXY_SOCKS4A_STATUS_CONFIRM_USERID		(NETPROXY_SOCKS4_STATUS_CONFIRM_USERID)

#define NETPROXY_SOCKS5_VERSION 					0x05
#define NETPROXY_SOCKS5_COMMAND_TCP_CONNECTION 		0x01
#define NETPROXY_SOCKS5_COMMAND_TCP_BIND 			0x02
#define NETPROXY_SOCKS5_COMMAND_UDP_ASSOCIATE 		0x0
#define NETPROXY_SOCKS5_STATUS_GRANTED 				0x00
#define NETPROXY_SOCKS5_STATUS_FAILURE 				0x01
#define NETPROXY_SOCKS5_STATUS_NOT_ALLOWED 			0x02
#define NETPROXY_SOCKS5_STATUS_NETWORK_UNREACHABLE 	0x03
#define NETPROXY_SOCKS5_STATUS_HOST_UNREACHABLE 	0x04
#define NETPROXY_SOCKS5_STATUS_REFUSED 				0x05
#define NETPROXY_SOCKS5_STATUS_TTL_EXPIRED 			0x06
#define NETPROXY_SOCKS5_STATUS_CMD_NOT_SUPPORTED 	0x07
#define NETPROXY_SOCKS5_STATUS_ADDR_NOT_SUPPORTED 	0x08
#define NETPROXY_SOCKS5_STATUS_NO_METHODS 			0xFF
#define NETPROXY_SOCKS5_AUTHTYPE_NO_AUTH 			0x00
#define NETPROXY_SOCKS5_AUTHTYPE_GSSAPI 			0x01
#define NETPROXY_SOCKS5_AUTHTYPE_USERNAME_PASSWORD 	0x02
#define NETPROXY_SOCKS5_AUTHTYPE_NO_METHODS 		0xFF
#define NETPROXY_SOCKS5_ADDRTYPE_IPV4 				0x1
#define NETPROXY_SOCKS5_ADDRTYPE_DOMAIN 			0x3
#define NETPROXY_SOCKS5_ADDRTYPE_IPV6 				0x4


enum NetProxy_t
{
	NetProxy_Http = 0,
	NetProxy_Socks4,
	NetProxy_Socks4a,
	NetProxy_Socks5,
};


struct NetProxyHttpParam_t
{
	char UserId[NETPROXY_NAME_MAX];
	char UserPw[NETPROXY_NAME_MAX];
};


struct NetProxySocks4Param_t
{
	char UserId[NETPROXY_NAME_MAX];
};


struct NetProxySocks4aParam_t
{
	char UserId[NETPROXY_NAME_MAX];
	char Domain[NETPROXY_NAME_MAX];
};


struct NetProxySocks5Param_t
{
	int32 Command;
	int32 Authtype[4];
	int32 AuthtypeNum;
	int32 AddrType;
	union
	{
		uint32 IPv4;
		uint8 IPv6[16];
		char Domain[NETPROXY_NAME_MAX];
	} Addr;
	char UserId[NETPROXY_NAME_MAX];	// valid for AUTHTYPE_USERNAME_PASSWORD, otherwise not using
	char UserPw[NETPROXY_NAME_MAX];	// valid for AUTHTYPE_USERNAME_PASSWORD, otherwise not using
};


union NetProxyParam_t
{
	NetProxyHttpParam_t Http;
	NetProxySocks4Param_t Socks4;
	NetProxySocks4aParam_t Socks4a;
	NetProxySocks5Param_t Socks5;
};