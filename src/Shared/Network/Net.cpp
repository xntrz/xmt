#include "Net.hpp"
#include "NetSsl.hpp"
#include "NetAddr.hpp"

#include "Tcp/TcpNetwork.hpp"


class CNet
{
public:
	inline CNet(void) {};
	inline ~CNet(void) {};
	inline CTcpNetwork& Tcp(void) { return m_TcpNetwork; };
	std::string UrlExtractPort(const std::string& Url);
	std::string UrlExtractProto(const std::string& Url);
	std::string UrlExtractDomain(const std::string& Url);

private:
	CTcpNetwork m_TcpNetwork;
};


std::string CNet::UrlExtractPort(const std::string& Url)
{
	auto p0 = Url.find("://");
	if (p0 != std::string::npos)
		p0 += 3;
	else
		p0 = 0;

	auto p1 = Url.find_first_of(':', p0);
	if (p1 != std::string::npos)
	{
		++p1;
		auto p2 = Url.find_first_of('/', p1);
		return Url.substr(p1, (p2 != std::string::npos ? (p2 - p1) : p2));
	};

	return{};
};


std::string CNet::UrlExtractProto(const std::string& Url)
{
	auto p0 = Url.find("://");
	if (p0 != std::string::npos)
		return Url.substr(0, p0);
	else
		return{};
};


std::string CNet::UrlExtractDomain(const std::string& Url)
{
	auto p0 = Url.find("://");
	if (p0 != std::string::npos)
		p0 += 3;
	else
		p0 = 0;

	auto p1 = Url.find_first_of(':', p0);
	if (p1 != std::string::npos)
		return Url.substr(p0, p1 - p0);

	auto p2 = Url.find_first_of('/', p0);
	if (p2 != std::string::npos)
		return Url.substr(p0, p2 - p0);

	return Url.substr(p0, p2);
};


static CNet* s_pNet = nullptr;


static inline CNet& Net(void)
{
	return *s_pNet;
};


bool NetInitialize(void)
{
	if (CNetSsl::Initialize(CNetSsl::CTXTYPE_TLS12))
	{
		s_pNet = new CNet;
		return true;
	};

	return false;
};


void NetTerminate(void)
{
	if (s_pNet)
	{
		delete s_pNet;
		s_pNet = nullptr;
	};
	
	CNetSsl::Terminate();
};


/*DLLSHARED*/ HCONN NetTcpOpen(NETEVENTPROC EventProc, void* Param)
{
	return Net().Tcp().Open(EventProc, Param);
};


/*DLLSHARED*/ void NetTcpClose(HCONN hConn)
{
	Net().Tcp().Close(hConn);
};


/*DLLSHARED*/ HCONN NetTcpCopy(HCONN hConn)
{
	return Net().Tcp().Copy(hConn);
};


/*DLLSHARED*/ HCONN NetTcpAccept(HCONN hConn)
{
	return Net().Tcp().Accept(hConn);
};


/*DLLSHARED*/ bool NetTcpConnect(HCONN hConn, uint64 NetAddr, uint32 Timeout)
{
	return NetTcpConnect(hConn, NetAddrIp(&NetAddr), NetAddrPort(&NetAddr), Timeout);
};


/*DLLSHARED*/ bool NetTcpConnect(HCONN hConn, uint32 Ip, uint16 Port, uint32 Timeout)
{
	return Net().Tcp().Connect(hConn, Ip, Port, Timeout);
};


/*DLLSHARED*/ bool NetTcpConnect(HCONN hConn, const char* Hostname, uint16 Port, uint32 Timeout)
{
	uint32 IpAddrArray[4] = { 0 };
	int32 IpAddrArrayCnt = COUNT_OF(IpAddrArray);

	if (Net().Tcp().GetAddrinfo(Hostname, IpAddrArray, &IpAddrArrayCnt))
		return NetTcpConnect(hConn, IpAddrArray[0], Port, Timeout);
	else
		return false;
};


/*DLLSHARED*/ bool NetTcpConnect(HCONN hConn, const char* Url, uint32 Timeout)
{
	uint16 Port = 0;

	std::string Domain = Net().UrlExtractDomain(Url);
	if (Domain.empty())
		return false;
	
	std::string Proto = Net().UrlExtractProto(Url);
	if ((Proto == "https") || (Proto == "wss"))
		Port = 443;

	if (Proto.empty())
	{
		std::string value = Net().UrlExtractPort(Url);
		if (!value.empty())
			Port = uint16(std::stoul(value));
	};

	if (!Port)
		Port = 80;

	return NetTcpConnect(hConn, Domain.c_str(), Port, Timeout);
};


/*DLLSHARED*/ bool NetTcpConnectSelf(HCONN hConn, uint16 Port, uint32 Timeout)
{
	uint32 IpAddrArray[4] = { 0 };
	int32 IpAddrArrayCount = COUNT_OF(IpAddrArray);

	if (NetGetAddrinfoSelf(IpAddrArray, &IpAddrArrayCount))
		return NetTcpConnect(hConn, IpAddrArray[0], Port, Timeout);
	else
		return false;
};


/*DLLSHARED*/ bool NetTcpCancelConnect(HCONN hConn)
{
	return Net().Tcp().CancelConnect(hConn);
};


/*DLLSHARED*/ bool NetTcpListen(HCONN hConn, int32 Backlog, uint64 NetAddr)
{
	return Net().Tcp().Listen(hConn, Backlog, NetAddrIp(&NetAddr), NetAddrPort(&NetAddr));
};


/*DLLSHARED*/ bool NetTcpListen(HCONN hConn, int32 Backlog, uint32 Ip, uint16 Port)
{
	return Net().Tcp().Listen(hConn, Backlog, Ip, Port);
};


/*DLLSHARED*/ bool NetTcpListen(HCONN hConn, int32 Backlog, const char* Ip, uint16 Port)
{
	uint64 NetAddr = 0;
	NetAddrInit(&NetAddr, Ip, Port);

	return NetTcpListen(hConn, Backlog, NetAddrIp(&NetAddr), NetAddrPort(&NetAddr));
};


/*DLLSHARED*/ bool NetTcpListenSelf(HCONN hConn, int32 Backlog, uint16 Port)
{
	uint32 IpAddrArray[4] = { 0 };
	int32 IpAddrArrayCount = COUNT_OF(IpAddrArray);

	if (NetGetAddrinfoSelf(IpAddrArray, &IpAddrArrayCount))
		return NetTcpListen(hConn, Backlog, IpAddrArray[0], Port);
	else
		return false;
};


/*DLLSHARED*/ uint32 NetTcpRecv(HCONN hConn, void* Buff, uint32 BuffSize)
{
	return Net().Tcp().Read(hConn, Buff, BuffSize);
};


/*DLLSHARED*/ uint32 NetTcpSend(HCONN hConn, const void* Data, uint32 DataSize, bool FlagDisconnectOnComplete)
{
	return Net().Tcp().Write(hConn, Data, DataSize, FlagDisconnectOnComplete);
};


/*DLLSHARED*/ bool NetTcpDisconnect(HCONN hConn)
{
	return Net().Tcp().Disconnect(hConn);
};


/*DLLSHARED*/ bool NetTcpResolve(HCONN hConn, const char* Hostname, uint32 Timeout)
{
	return Net().Tcp().Resolve(hConn, Hostname, Timeout);
};


/*DLLSHARED*/ bool NetTcpCancelResolve(HCONN hConn)
{
	return Net().Tcp().CancelResolve(hConn);
};


/*DLLSHARED*/ void NetTcpSetForceClose(HCONN hConn, bool Flag)
{
	Net().Tcp().SetForceClose(hConn, Flag);
};


/*DLLSHARED*/ void NetTcpSetTimeoutRead(HCONN hConn, uint32 Timeout)
{
	Net().Tcp().SetTimeoutRead(hConn, Timeout);
};


/*DLLSHARED*/ void NetTcpSetUserParam(HCONN hConn, void* userparam)
{
	Net().Tcp().SetUserParam(hConn, userparam);
};


/*DLLSHARED*/ void* NetTcpGetUserParam(HCONN hConn)
{
	return Net().Tcp().GetUserParam(hConn);
};


/*DLLSHARED*/ void NetTcpSetProxy(HCONN hConn, NETPROXY ProxyType, uint64 NetAddr, const NETPROXYPARAM* Parameter)
{
	Net().Tcp().SetProxy(hConn, ProxyType, NetAddr, Parameter);
};


/*DLLSHARED*/ void NetTcpClearProxy(HCONN hConn)
{
	Net().Tcp().ClearProxy(hConn);
};


/*DLLSHARED*/ void NetTcpSetSecure(HCONN hConn, bool Flag)
{
	Net().Tcp().SetSecurity(hConn, Flag);
};


/*DLLSHARED*/ void NetTcpSetSecureHost(HCONN hConn, const char* Hostname)
{
	Net().Tcp().SetSecurityHost(hConn, Hostname);
};


/*DLLSHARED*/ bool NetTcpIsRemote(HCONN hConn)
{
	return Net().Tcp().IsRemote(hConn);
};


/*DLLSHARED*/ bool NetTcpIsConnected(HCONN hConn)
{
	return Net().Tcp().IsConnected(hConn);
};


/*DLLSHARED*/ uint64 NetTcpNetAddr(HCONN hConn)
{
	return Net().Tcp().NetAddr(hConn);
};


/*DLLSHARED*/ void NetTcpSetEventProc(HCONN hConn, NETEVENTPROC EventProc, void* Param)
{
	Net().Tcp().SetEventProc(hConn, EventProc, Param);
};


/*DLLSHARED*/ void NetTcpGetEventProc(HCONN hConn, NETEVENTPROC* EventProc, void** Param)
{
	Net().Tcp().GetEventProc(hConn, EventProc, Param);
};


/*DLLSHARED*/ bool NetGetAddrinfo(const char* Hostname, uint32* IpAddrArray, int32* IpAddrArrayCount)
{
	return Net().Tcp().GetAddrinfo(Hostname, IpAddrArray, IpAddrArrayCount);
};


/*DLLSHARED*/ bool NetGetAddrinfo(const char* Hostname, uint32* Ip)
{
	return Net().Tcp().GetAddrinfo(Hostname, Ip);
};


/*DLLSHARED*/ bool NetGetAddrinfoSelf(uint32* IpAddrArray, int32* IpAddrArrayCount)
{
	return Net().Tcp().GetAddrinfoSelf(IpAddrArray, IpAddrArrayCount);
};