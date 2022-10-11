#pragma once

struct NetSettings_t
{
    int32 TcpAcceptActPoolSize;
    int32 TcpWriteActPoolSize;
    int32 TcpSockPoolSize;
    int32 TcpIoThreads;
    uint32 TcpReadTimeout;
    uint32 TcpDcTimeout;
    uint32 TcpConnDefTimeout;
    char SslPathCert[256];
    char SslPathKey[256];
};

extern NetSettings_t NetSettings;


void NetSettingsInitialize(void);
void NetSettingsTerminate(void);