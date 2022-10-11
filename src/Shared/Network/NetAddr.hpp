#pragma once


DLLSHARED void NetAddrInit(uint64* NetAddr);
DLLSHARED void NetAddrInit(uint64* NetAddr, const char* Ip, uint16 Port);
DLLSHARED void NetAddrInit(uint64* NetAddr, uint32 Ip, uint16 Port);
DLLSHARED bool NetAddrIsValid(uint64* NetAddr);
DLLSHARED bool NetAddrIsInNetOrder(uint64* NetAddr);
DLLSHARED bool NetAddrIsInHostOrder(uint64* NetAddr);
DLLSHARED void NetAddrIp(uint64* NetAddr, uint32 Ip);
DLLSHARED uint32 NetAddrIp(uint64* NetAddr);
DLLSHARED void NetAddrPort(uint64* NetAddr, uint16 Port);
DLLSHARED uint16 NetAddrPort(uint64* NetAddr);
DLLSHARED void NetAddrHton(uint64* NetAddr);
DLLSHARED void NetAddrNtoh(uint64* NetAddr);
DLLSHARED void NetAddrToString(uint64* NetAddr, char* Buffer, int32 BufferSize, char IpPortDelimiter = ':');
DLLSHARED void NetAddrToStringIp(uint64* NetAddr, char* Buffer, int32 BufferSize);
DLLSHARED void NetAddrToStringPort(uint64* NetAddr, char* Buffer, int32 BufferSize);
DLLSHARED std::string NetAddrToStdString(uint64* NetAddr, char IpPortDelimiter = ':');
DLLSHARED std::string NetAddrToStdStringIp(uint64* NetAddr);
DLLSHARED std::string NetAddrToStdStringPort(uint64* NetAddr);
DLLSHARED bool NetAddrIsValidIp(const char* IpAddress);