#include "NetSettings.hpp"

#include "Shared/Common/Registry.hpp"


/*extern*/ NetSettings_t NetSettings =
{    
    /*TcpAcceptActPoolSize  */  1024,
    /*TcpWriteActPoolSize   */  4096,
    /*TcpSockPoolSize       */  4096,
    /*TcpIoThreads          */  -1,
    /*TcpReadTimeout        */  10000,
    /*TcpDcTimeout          */  10000,
    /*TcpConnDefTimeout     */  10000,
    /*SslPathCert[256]      */  "",
    /*SslPathKey[256]       */  "",
};


static void NetSettingsLoad(void)
{
    HOBJ hVar = 0;

    hVar = RegVarFind("tcp_accept_workpool");
    if (hVar)
        NetSettings.TcpAcceptActPoolSize = RegVarReadInt32(hVar);

    hVar = RegVarFind("tcp_write_workpool");
    if (hVar)
        NetSettings.TcpWriteActPoolSize = RegVarReadInt32(hVar);

    hVar = RegVarFind("tcp_sock_workpool");
    if (hVar)
        NetSettings.TcpSockPoolSize = RegVarReadInt32(hVar);

    hVar = RegVarFind("tcp_io_workpool");
    if (hVar)
        NetSettings.TcpIoThreads = RegVarReadInt32(hVar);

    hVar = RegVarFind("tcp_tmout_read");
    if (hVar)
        NetSettings.TcpReadTimeout = RegVarReadUInt32(hVar);

    hVar = RegVarFind("tcp_tmout_conn");
    if (hVar)
        NetSettings.TcpConnDefTimeout = RegVarReadUInt32(hVar);

    hVar = RegVarFind("tcp_tmout_dc");
    if (hVar)
        NetSettings.TcpDcTimeout = RegVarReadUInt32(hVar);

#ifdef _DEBUG    
    hVar = RegVarFind("ssl_path_cert");
    if (hVar)
        RegVarReadString(hVar, NetSettings.SslPathCert, sizeof(NetSettings.SslPathCert));

    hVar = RegVarFind("ssl_path_key");
    if (hVar)
        RegVarReadString(hVar, NetSettings.SslPathKey, sizeof(NetSettings.SslPathKey));
#endif
};


static void NetSettingsCheck(void)
{
    if (NetSettings.TcpAcceptActPoolSize < 1)
        NetSettings.TcpAcceptActPoolSize = 1024;

    if (NetSettings.TcpWriteActPoolSize < 1024)
        NetSettings.TcpWriteActPoolSize = 4096;

    if (NetSettings.TcpSockPoolSize < 0)
        NetSettings.TcpSockPoolSize = 0;

    if (NetSettings.TcpIoThreads > 512)
        NetSettings.TcpSockPoolSize = -1;
};


void NetSettingsInitialize(void)
{
    RegResetRegist(NetSettingsLoad);
    NetSettingsLoad();
    NetSettingsCheck();
};


void NetSettingsTerminate(void)
{
    ;
};