#include "NetAddr.hpp"
#include "NetTypedefs.hpp"


enum NetAddrFlag_t : uint16
{
    NetAddrFlag_Invalid = BIT(0),
    NetAddrFlag_OrderNet = BIT(1),
    NetAddrFlag_OrderHost = BIT(2),
};


union NetAddr_t
{
    uint64 Value;
    struct
    {
        uint32 Ip;
        uint16 Port;
        uint16 Flags;
    } Parts;

    inline NetAddr_t(uint64 AddrValue)
    : Value(AddrValue)
    {
        ;
    };
    
    inline NetAddr_t(uint32 Ip, uint16 Port)
    : Parts({ Ip, Port, 0 })
    {
        ;
    };

    inline NetAddr_t(uint32 Ip, uint16 Port, uint16 Flags)
    : Parts({Ip, Port, Flags})
    {
        ;
    };

    inline operator uint64(void)
    {
        return Value;
    };
};


static_assert(sizeof(NetAddr_t) == sizeof(uint64), "check me");


static uint64 NetAddrInvalid = NetAddr_t(uint32_max, uint16_max, NetAddrFlag_Invalid);


void NetAddrInit(uint64* NetAddr)
{
    *NetAddr = NetAddrInvalid;
};


void NetAddrInit(uint64* NetAddr, const char* Ip, uint16 Port)
{
    *NetAddr = NetAddr_t(ntohl(inet_addr(Ip)), Port);
};


void NetAddrInit(uint64* NetAddr, uint32 Ip, uint16 Port)
{
    *NetAddr = NetAddr_t(Ip, Port);
};


bool NetAddrIsValid(uint64* NetAddr)
{
    NetAddr_t Addr(*NetAddr);
    return !IS_FLAG_SET(Addr.Parts.Flags, NetAddrFlag_Invalid);
};


bool NetAddrIsInNetOrder(uint64* NetAddr)
{
    NetAddr_t Addr(*NetAddr);
    return IS_FLAG_SET(Addr.Parts.Flags, NetAddrFlag_OrderNet);
};


bool NetAddrIsInHostOrder(uint64* NetAddr)
{
    NetAddr_t Addr(*NetAddr);
    return IS_FLAG_SET(Addr.Parts.Flags, NetAddrFlag_OrderHost);
};


void NetAddrIp(uint64* NetAddr, uint32 Ip)
{
    NetAddr_t Addr(*NetAddr);
    Addr.Parts.Ip = Ip;
    FLAG_CLEAR(Addr.Parts.Flags, NetAddrFlag_Invalid);
    *NetAddr = Addr;
};


uint32 NetAddrIp(uint64* NetAddr)
{
    NetAddr_t Addr(*NetAddr);
    return Addr.Parts.Ip;
};


void NetAddrPort(uint64* NetAddr, uint16 Port)
{
    NetAddr_t Addr(*NetAddr);
    Addr.Parts.Port = Port;
    FLAG_CLEAR(Addr.Parts.Flags, NetAddrFlag_Invalid);
    *NetAddr = Addr;
};


uint16 NetAddrPort(uint64* NetAddr)
{
    NetAddr_t Addr(*NetAddr);
    return Addr.Parts.Port;
};


void NetAddrHton(uint64* NetAddr)
{
    NetAddr_t Addr(*NetAddr);
    Addr.Parts.Ip = htonl(Addr.Parts.Ip);
    Addr.Parts.Port = htons(Addr.Parts.Port);
    FLAG_CLEAR(Addr.Parts.Flags, NetAddrFlag_OrderHost);
    FLAG_SET(Addr.Parts.Flags, NetAddrFlag_OrderNet);
    *NetAddr = Addr;
};


void NetAddrNtoh(uint64* NetAddr)
{
    NetAddr_t Addr(*NetAddr);
    Addr.Parts.Ip = ntohl(Addr.Parts.Ip);
    Addr.Parts.Port = ntohs(Addr.Parts.Port);
    FLAG_CLEAR(Addr.Parts.Flags, NetAddrFlag_OrderNet);
    FLAG_SET(Addr.Parts.Flags, NetAddrFlag_OrderHost);
    *NetAddr = Addr;
};


void NetAddrToString(uint64* NetAddr, char* Buffer, int32 BufferSize, char IpPortDelimiter)
{
    char IpAddress[32];
    IpAddress[0] = '\0';

    char Port[32];
    Port[0] = '\0';

    NetAddrToStringIp(NetAddr, IpAddress, sizeof(IpAddress));
    NetAddrToStringPort(NetAddr, Port, sizeof(Port));

    sprintf_s(Buffer, BufferSize, "%s%c%s", IpAddress, IpPortDelimiter, Port);
};


void NetAddrToStringIp(uint64* NetAddr, char* Buffer, int32 BufferSize)
{
    in_addr InAddr = {};
    if (NetAddrIsInNetOrder(NetAddr))
        InAddr.S_un.S_addr = NetAddr_t(*NetAddr).Parts.Ip;
    else
        InAddr.S_un.S_addr = htonl(NetAddr_t(*NetAddr).Parts.Ip);

    const char* IpAddressAsStr = inet_ntoa(InAddr);
    ASSERT(IpAddressAsStr);

    sprintf_s(Buffer, BufferSize, "%s", IpAddressAsStr);
};


void NetAddrToStringPort(uint64* NetAddr, char* Buffer, int32 BufferSize)
{
    sprintf_s(Buffer, BufferSize, "%hu", NetAddr_t(*NetAddr).Parts.Port);
};


std::string NetAddrToStdString(uint64* NetAddr, char IpPortDelimiter)
{
    return std::string(
        NetAddrToStdStringIp(NetAddr) + IpPortDelimiter + NetAddrToStdStringPort(NetAddr)
    );
};


std::string NetAddrToStdStringIp(uint64* NetAddr)
{
    in_addr InAddr = {};
    if (NetAddrIsInNetOrder(NetAddr))
        InAddr.S_un.S_addr = NetAddr_t(*NetAddr).Parts.Ip;
    else        
        InAddr.S_un.S_addr = htonl(NetAddr_t(*NetAddr).Parts.Ip);

    const char* IpAddressAsStr = inet_ntoa(InAddr);
    ASSERT(IpAddressAsStr);

    return std::string(IpAddressAsStr);
};


std::string NetAddrToStdStringPort(uint64* NetAddr)
{
    return std::string(
        std::to_string(NetAddr_t(*NetAddr).Parts.Port)
    );
};


bool NetAddrIsValidIp(const char* IpAddress)
{
    IN_ADDR InAddr = {};
    return (inet_pton(AF_INET, IpAddress, &InAddr) == 1);
};