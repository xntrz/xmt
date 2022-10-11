#include "Net.hpp"
#include "NetSSL.hpp"
#include "NetAddr.hpp"
#include "NetSockUtils.hpp"
#include "NetSettings.hpp"
#include "TcpNetwork.hpp"


bool NetInitialize(void)
{
	NetSettingsInitialize();
	NetWsaInitialize();
	if (NetWsaStart())
	{
		NetSslInitialize(NetSslCtxType_tls12);
		TcpNetInitialize();
		
		return true;
	}
	else
	{
		NetWsaTerminate();
		
		return false;
	};
};


void NetTerminate(void)
{
	TcpNetTerminate();
	NetSslTerminate();
	NetWsaStop();
	NetWsaTerminate();
	NetSettingsTerminate();
};


void NetTcpSetNotifySent(HOBJ hConn, bool Flag)
{
	TcpNetSetNotifySent(hConn, Flag);
};


void NetTcpSetForceClose(HOBJ hConn, bool Flag)
{
	TcpNetSetForceClose(hConn, Flag);
};


bool NetTcpSetTimeoutRead(HOBJ hConn, uint32 Timeout)
{
	return TcpNetSetTimeoutRead(hConn, Timeout);
};


void NetTcpSetUserParam(HOBJ hConn, void* userparam)
{
	TcpNetSetUserParam(hConn, userparam);
};


void* NetTcpGetUserParam(HOBJ hConn)
{
	return TcpNetGetUserParam(hConn);
};


bool NetTcpSetProxy(HOBJ hConn, NetProxy_t ProxyType, uint64 NetAddr, void* Parameter, int32 ParameterLen)
{
	return TcpNetSetProxy(hConn, ProxyType, NetAddr, Parameter, ParameterLen);
};


bool NetTcpClearProxy(HOBJ hConn)
{
	return TcpNetClearProxy(hConn);
};


bool NetTcpSetSecure(HOBJ hConn, bool Flag)
{
	return TcpNetSetSecurity(hConn, Flag);
};


bool NetTcpSetSecureHost(HOBJ hConn, const char* Hostname)
{
	return TcpNetSetSecurityHost(hConn, Hostname);
};


HOBJ NetTcpOpen(NetEventProc_t EventProc, void* Param)
{
	return TcpNetOpen(EventProc, Param);
};


void NetTcpClose(HOBJ hConn)
{
	TcpNetClose(hConn);
};


HOBJ NetTcpCopy(HOBJ hConn)
{
	return TcpNetCopy(hConn);
};


bool NetTcpConnect(HOBJ hConn, uint64 NetAddr, uint32 Timeout)
{
	return NetTcpConnect(hConn, NetAddrIp(&NetAddr), NetAddrPort(&NetAddr), Timeout);
};


bool NetTcpConnect(HOBJ hConn, uint32 Ip, uint16 Port, uint32 Timeout)
{
	return TcpNetConnect(hConn, Ip, Port, Timeout);
};


bool NetTcpConnect(HOBJ hConn, const char* Hostname, uint16 Port, uint32 Timeout)
{
	uint32 IpAddrArray[4] = { 0 };
	int32 IpAddrArrayCount = COUNT_OF(IpAddrArray);

	if (NetGetAddrinfo(Hostname, IpAddrArray, &IpAddrArrayCount))
		return NetTcpConnect(hConn, IpAddrArray[0], Port, Timeout);
	else
		return false;
};


bool NetTcpConnectSelf(HOBJ hConn, uint16 Port, uint32 Timeout)
{
	uint32 IpAddrArray[4] = { 0 };
	int32 IpAddrArrayCount = COUNT_OF(IpAddrArray);

	if (NetGetAddrinfoSelf(IpAddrArray, &IpAddrArrayCount))
		return NetTcpConnect(hConn, IpAddrArray[0], Port, Timeout);
	else
		return false;
};


bool NetTcpCancelConnect(HOBJ hConn)
{
	return TcpNetCancelConnect(hConn);
};


bool NetTcpListen(HOBJ hConn, uint64 NetAddr)
{	
	return TcpNetListen(hConn, NetAddrIp(&NetAddr), NetAddrPort(&NetAddr));
};


bool NetTcpListen(HOBJ hConn, uint32 Ip, uint16 Port)
{
	return TcpNetListen(hConn, Ip, Port);
};


bool NetTcpListen(HOBJ hConn, const char* Ip, uint16 Port)
{
	uint64 NetAddr = 0;
	NetAddrInit(&NetAddr, Ip, Port);

	return NetTcpListen(hConn, NetAddrIp(&NetAddr), NetAddrPort(&NetAddr));
};


bool NetTcpListenSelf(HOBJ hConn, uint16 Port)
{
	uint32 IpAddrArray[4] = { 0 };
	int32 IpAddrArrayCount = COUNT_OF(IpAddrArray);
	
	if (NetGetAddrinfoSelf(IpAddrArray, &IpAddrArrayCount))
		return NetTcpListen(hConn, IpAddrArray[0], Port);
	else
		return false;
};


bool NetTcpListenEnable(HOBJ hConn, bool Flag)
{
	return TcpNetListenEnable(hConn, Flag);
};


bool NetTcpSend(HOBJ hConn, const char* Data, int32 DataSize, bool FlagDisconnectOnComplete)
{
	return TcpNetWrite(hConn, Data, DataSize, FlagDisconnectOnComplete);
};


bool NetTcpDisconnect(HOBJ hConn)
{
	return TcpNetDisconnect(hConn);
};


bool NetGetAddrinfo(const char* Hostname, uint32* IpAddrArray, int32* IpAddrArrayCount)
{
	return NetDnsResolve(Hostname, IpAddrArray, IpAddrArrayCount);
};


bool NetGetAddrinfo(const char* Hostname, uint32* Ip)
{
	uint32 IpAddrArray[32] = { 0 };	
	int32 IpAddrArrayCount = COUNT_OF(IpAddrArray);

	if (NetGetAddrinfo(Hostname, IpAddrArray, &IpAddrArrayCount))
	{
		*Ip = IpAddrArray[0];
		return true;
	};

	return false;
};


bool NetGetAddrinfoSelf(uint32* IpAddrArray, int32* IpAddrArrayCount)
{
	char Hostname[256];
	Hostname[0] = '\0';

	if (!gethostname(Hostname, sizeof(Hostname)))
	{
		if (NetGetAddrinfo(Hostname, IpAddrArray, IpAddrArrayCount))
			return true;
	};

	return false;
};

