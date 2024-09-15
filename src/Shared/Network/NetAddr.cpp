#include "NetAddr.hpp"

#include "Tcp/TcpTypedefs.hpp"


class CNetAddr
{
public:
    CNetAddr(void);
    CNetAddr(uint64* NetAddr);
    CNetAddr(uint64 Value);
    CNetAddr(uint32 Ip, uint16 Port);
    CNetAddr(uint32 Ip, uint16 Port, uint16 Flags);
    CNetAddr& Ip(uint32 Ip);
    CNetAddr& Port(uint16 Port);
    uint32 Ip(void) const;
    uint16 Port(void) const;
    uint64 Value(void) const;
    operator uint64(void) const;
    CNetAddr& hton(void);
    CNetAddr& ntoh(void);
    bool validate(void) const;

private:
    union
    {
        uint64 m_Value;
        struct
        {
            union
            {
                uint8 m_IpBytes[4];
                uint32 m_Ip;
            };        
            uint16 m_Port;
        };        
    };
};


static_assert(sizeof(CNetAddr) == sizeof(uint64), "check me");


CNetAddr::CNetAddr(void)
: m_Ip(uint32_max)
, m_Port(uint16_max)
{
    ;
};


CNetAddr::CNetAddr(uint64 Value)
: m_Value(Value)
{
    ;
};


CNetAddr::CNetAddr(uint32 Ip, uint16 Port)
: m_Ip(Ip)
, m_Port(Port)
{
    ;
};


CNetAddr::CNetAddr(uint32 Ip, uint16 Port, uint16 Flags)
: m_Ip(Ip)
, m_Port(Port)
{
    ;
};


CNetAddr& CNetAddr::Ip(uint32 Ip)
{
    m_Ip = Ip;
    return *this;
};


CNetAddr& CNetAddr::Port(uint16 Port)
{
    m_Port = Port;
    return *this;
};


uint32 CNetAddr::Ip(void) const
{
    return m_Ip;
};


uint16 CNetAddr::Port(void) const
{
    return m_Port;
};


uint64 CNetAddr::Value(void) const
{
    return m_Value;
};


CNetAddr::operator uint64(void) const
{
    return Value();
};


CNetAddr& CNetAddr::hton(void)
{
    m_Ip = htonl(m_Ip);
    m_Port = htons(m_Port);

    return *this;
};


CNetAddr& CNetAddr::ntoh(void)
{
    m_Ip = ntohl(m_Ip);
    m_Port = ntohs(m_Port);

    return *this;
};


bool CNetAddr::validate(void) const
{
    in_addr InAddr = { 0 };
    InAddr.S_un.S_un_b.s_b1 = m_IpBytes[0];
    InAddr.S_un.S_un_b.s_b2 = m_IpBytes[1];
    InAddr.S_un.S_un_b.s_b3 = m_IpBytes[2];
    InAddr.S_un.S_un_b.s_b4 = m_IpBytes[3];
    
    return (inet_ntoa(InAddr) != nullptr);
};


/*DLLSHARED*/ void NetAddrInit(uint64* NetAddr)
{
    *NetAddr = 0;
};


/*DLLSHARED*/ void NetAddrInit(uint64* NetAddr, const char* Ip, uint16 Port)
{
    *NetAddr = CNetAddr(ntohl(inet_addr(Ip)), Port);
};


/*DLLSHARED*/ void NetAddrInit(uint64* NetAddr, uint32 Ip, uint16 Port)
{
    *NetAddr = CNetAddr(Ip, Port);
};


/*DLLSHARED*/ bool NetAddrIsValid(uint64* NetAddr)
{
    return CNetAddr(*NetAddr).validate();
};


/*DLLSHARED*/ void NetAddrIp(uint64* NetAddr, uint32 Ip)
{
    *NetAddr = CNetAddr(*NetAddr).Ip(Ip);
};


/*DLLSHARED*/ uint32 NetAddrIp(uint64* NetAddr)
{
    return CNetAddr(*NetAddr).Ip();
};


/*DLLSHARED*/ void NetAddrPort(uint64* NetAddr, uint16 Port)
{
    *NetAddr = CNetAddr(*NetAddr).Port(Port);
};


/*DLLSHARED*/ uint16 NetAddrPort(uint64* NetAddr)
{
    return CNetAddr(*NetAddr).Port();
};


/*DLLSHARED*/ void NetAddrHton(uint64* NetAddr)
{
    *NetAddr = CNetAddr(*NetAddr).hton();
};


/*DLLSHARED*/ void NetAddrNtoh(uint64* NetAddr)
{
    *NetAddr = CNetAddr(*NetAddr).ntoh();    
};


/*DLLSHARED*/ void NetAddrToString(uint64* NetAddr, char* Buffer, int32 BufferSize, char IpPortDelimiter)
{
    char IpAddress[32];
    IpAddress[0] = '\0';

    char Port[32];
    Port[0] = '\0';

    NetAddrToStringIp(NetAddr, IpAddress, sizeof(IpAddress));
    NetAddrToStringPort(NetAddr, Port, sizeof(Port));

    std::sprintf(Buffer, "%s%c%s", IpAddress, IpPortDelimiter, Port);
};


/*DLLSHARED*/ void NetAddrToStringIp(uint64* NetAddr, char* Buffer, int32 BufferSize)
{
    in_addr InAddr = {};
    InAddr.s_addr = CNetAddr(*NetAddr).Ip();

    const char* IpAddressAsStr = inet_ntoa(InAddr);
    ASSERT(IpAddressAsStr);
    if (IpAddressAsStr)
        std::sprintf(Buffer, "%s", IpAddressAsStr);
};


/*DLLSHARED*/ void NetAddrToStringPort(uint64* NetAddr, char* Buffer, int32 BufferSize)
{
    std::sprintf(Buffer, "%" PRIu16, CNetAddr(*NetAddr).Port());
};


/*DLLSHARED*/ std::string NetAddrToStdString(uint64* NetAddr, char IpPortDelimiter)
{
    return std::string(NetAddrToStdStringIp(NetAddr) + IpPortDelimiter + NetAddrToStdStringPort(NetAddr));
};


/*DLLSHARED*/ std::string NetAddrToStdStringIp(uint64* NetAddr)
{
    in_addr InAddr = {};
    InAddr.s_addr = CNetAddr(*NetAddr).Ip();
    InAddr.s_addr = byteswap4(InAddr.s_addr);// ntohl(InAddr.s_addr);

    const char* IpAddressAsStr = inet_ntoa(InAddr);    
    ASSERT(IpAddressAsStr);
    if (IpAddressAsStr)
        return std::string(IpAddressAsStr);
    else
        return {};
};


/*DLLSHARED*/ std::string NetAddrToStdStringPort(uint64* NetAddr)
{
    return std::to_string(CNetAddr(*NetAddr).Port());
};


/*DLLSHARED*/ bool NetAddrIsValidIp(const char* IpAddress)
{
    in_addr InAddr = {};
    return (inet_pton(AF_INET, IpAddress, &InAddr) == 1);
};