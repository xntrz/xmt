#include "TcpSsl.hpp"
#include "TcpLayer.hpp"
#include "NetSsl.hpp"
#include "NetAddr.hpp"


static const int32 TCPSSL_BUFFSIZE_READ = 4096;
static const int32 TCPSSL_BUFFSIZE_WRITE = 4096;
static const int32 TCPSSL_TLS_RECORD_SIZE = 16384;


struct TcpSsl_t
{
    TcpLayer_t Layer;
    BIO* BioR;
    BIO* BioW;
    SSL* Ctx;
    uint64 NetAddr;
    char Hostname[256];
    std::atomic<bool> FlagHandshakeComplete;
    std::atomic<bool> FlagCloseNotifySent;
};


static void TcpSslStartupPerSession(TcpSsl_t* TcpSsl);
static void TcpSslCleanupPerSession(TcpSsl_t* TcpSsl);
static void TcpSslStartupPerConnect(TcpSsl_t* TcpSsl);
static bool TcpSslHandshakeInitiate(TcpSsl_t* TcpSsl);
static bool TcpSslHandshakeProcess(TcpSsl_t* TcpSsl);
static bool TcpSslCtxCreate(TcpSsl_t* TcpSsl);
static void TcpSslCtxDestroy(TcpSsl_t* TcpSsl);
static bool TcpSslRead(TcpSsl_t* TcpSsl, const char* Data = nullptr, int32 DataSize = 0);
static bool TcpSslWrite(TcpSsl_t* TcpSsl, const char* Data = nullptr, int32 DataSize = 0, bool FlagDisconnectOnComplete = false);
static bool TcpSslDisconnect(TcpSsl_t* TcpSsl);
static bool TcpSslEventProc(TcpLayer_t* TcpLayer, int32 Id, const void* Param);
static bool TcpSslCmdProc(TcpLayer_t* TcpLayer, int32 Id, void* Param);


static void TcpSslStartupPerSession(TcpSsl_t* TcpSsl)
{
    TcpSsl->Layer.data = &TcpSsl->Layer;
    TcpSsl->Layer.FnEventProc = TcpSslEventProc;
    TcpSsl->Layer.FnCmdProc = TcpSslCmdProc;
    TcpSsl->BioR = nullptr;
    TcpSsl->BioW = nullptr;
    TcpSsl->Ctx = nullptr;
    NetAddrInit(&TcpSsl->NetAddr);
    TcpSsl->Hostname[0] = '\0';
    TcpSsl->FlagHandshakeComplete = false;
    TcpSsl->FlagCloseNotifySent = false;
};


static void TcpSslCleanupPerSession(TcpSsl_t* TcpSsl)
{
    TcpSslCtxDestroy(TcpSsl);
};


static void TcpSslStartupPerConnect(TcpSsl_t* TcpSsl)
{
    TcpSslCtxCreate(TcpSsl);
    TcpSsl->FlagHandshakeComplete = false;
};


static bool TcpSslHandshakeInitiate(TcpSsl_t* TcpSsl)
{
    if (TcpLayerIsRemote(&TcpSsl->Layer))
        SSL_set_accept_state(TcpSsl->Ctx);
    else
        SSL_set_connect_state(TcpSsl->Ctx);

    return TcpSslRead(TcpSsl);
};


static bool TcpSslHandshakeProcess(TcpSsl_t* TcpSsl)
{
    return TcpSslWrite(TcpSsl);
};


static bool TcpSslCtxCreate(TcpSsl_t* TcpSsl)
{
    if (!TcpSsl->Ctx)
    {
        ASSERT(!TcpSsl->BioR);
        ASSERT(!TcpSsl->BioW);

        TcpSsl->Ctx = NetSslAlloc();
        ASSERT(TcpSsl->Ctx);

        TcpSsl->BioR = BIO_new(BIO_s_mem());
        ASSERT(TcpSsl->BioR);
        
        TcpSsl->BioW = BIO_new(BIO_s_mem());
        ASSERT(TcpSsl->BioW);

        BIO_set_write_buf_size(TcpSsl->BioR, TCPSSL_BUFFSIZE_READ);
        BIO_set_write_buf_size(TcpSsl->BioW, TCPSSL_BUFFSIZE_WRITE);
        
        SSL_set_bio(TcpSsl->Ctx, TcpSsl->BioR, TcpSsl->BioW);
    }
    else
    {
        SSL_clear(TcpSsl->Ctx);        
    };

    if (TcpSsl->Hostname[0] != '\0')
    {
        if (TcpSsl->Ctx)
            SSL_set_tlsext_host_name(TcpSsl->Ctx, TcpSsl->Hostname);
    };

    return true;
};


static void TcpSslCtxDestroy(TcpSsl_t* TcpSsl)
{
    if (TcpSsl->Ctx)
    {
        ASSERT(TcpSsl->BioR);
        ASSERT(TcpSsl->BioW);

        NetSslShutdown(TcpSsl->Ctx);
        NetSslFree(TcpSsl->Ctx);

        TcpSsl->BioR = nullptr;
        TcpSsl->BioW = nullptr;
        TcpSsl->Ctx = nullptr;
    };
};


static bool TcpSslRead(TcpSsl_t* TcpSsl, const char* Data, int32 DataSize)
{
    int32 Result = 0;
    int32 Bytes = 0;
    
    if (DataSize)
    {
        Bytes = BIO_write(TcpSsl->BioR, Data, DataSize);
        Result = NetSslGetError(TcpSsl->Ctx, Bytes);
        if ((Bytes != DataSize) && (!BIO_should_retry(TcpSsl->BioR)))
        {            
            ASSERT(false);
            return false;
        };
    };

    char Buffer[TCPSSL_BUFFSIZE_READ];
    int32 Readed = 0;    
    do
    {
        Bytes = SSL_read(TcpSsl->Ctx, Buffer, sizeof(Buffer));
        Result = NetSslGetError(TcpSsl->Ctx, Bytes);
        if (Bytes > 0)
        {
            Readed += Bytes;

            TcpLayerEventParamRead_t ReadParam = {};
            ReadParam.Data = Buffer;
            ReadParam.DataSize = Bytes;
            ReadParam.ErrorCode = 0;
            ReadParam.Received = DataSize;

            if (!TcpLayerSendEvent(&TcpSsl->Layer, TcpLayerEvent_Read, &ReadParam))
                return false;
        }
        else if (NetSslIsFatalError(Result))
        {
            return false;
        }
        else if (NetSslIsEOF(Result))
        {
            ASSERT(Bytes == 0);
            //OUTPUTLN("Grace SSL clean!");
            return true;
        };

        if (!TcpSsl->FlagHandshakeComplete)
        {
            if (SSL_is_init_finished(TcpSsl->Ctx))
            {
                TcpSsl->FlagHandshakeComplete = true;

                TcpLayerEventParamConnect_t ConnectParam = {};
                ConnectParam.Ip = NetAddrIp(&TcpSsl->NetAddr);
                ConnectParam.Port = NetAddrIp(&TcpSsl->NetAddr);
                ConnectParam.FlagRemote = TcpLayerIsRemote(&TcpSsl->Layer);

                if (TcpLayerSendEvent(&TcpSsl->Layer, TcpLayerEvent_Connect, &ConnectParam))
                {
                    if (TcpLayerIsRemote(&TcpSsl->Layer))
                        TcpSslHandshakeProcess(TcpSsl);
                }
                else
                {
                    TcpSslDisconnect(TcpSsl);
                };

                break;
            }
            else
            {
                TcpSslHandshakeProcess(TcpSsl);
            };
        };
    } while (Bytes > 0);

    return true;
};


static bool TcpSslWrite(TcpSsl_t* TcpSsl, const char* Data, int32 DataSize, bool FlagDisconnectOnComplete)
{
    int32 Result = 0;
    int32 Bytes = 0;

    if (DataSize)
    {
        Bytes = SSL_write(TcpSsl->Ctx, Data, DataSize);
        Result = NetSslGetError(TcpSsl->Ctx, Bytes);
        if ((Bytes != DataSize) || NetSslIsFatalError(Result))
        {
            ASSERT(false);
            return false;
        };
    };

    char Buffer[TCPSSL_BUFFSIZE_WRITE];
    int32 Written = 0;
    
    while (BIO_pending(TcpSsl->BioW))
    {
        Bytes = BIO_read(TcpSsl->BioW, Buffer + Written, sizeof(Buffer) - Written);
        Result = NetSslGetError(TcpSsl->Ctx, Bytes);
        if (NetSslIsFatalError(Result))
        {
            ASSERT(false);
            return false;
        }
        else
        {
            Written += Bytes;
        };

        if (Written)
        {
            TcpLayerCmdParamWrite_t WriteParam = {};
            WriteParam.Data = Buffer;
            WriteParam.DataSize = Written;
            WriteParam.FlagDisconnectOnComplete = FlagDisconnectOnComplete;

            if (!TcpLayerSendCmd(&TcpSsl->Layer, TcpLayerCmd_Write, &WriteParam))
                return false;

            Written = 0;
        };
    };

    return true;
};


static bool TcpSslDisconnect(TcpSsl_t* TcpSsl)
{
    TcpLayerCmdParamDisconnect_t Param = {};
    
    return TcpLayerSendCmd(&TcpSsl->Layer, TcpLayerCmd_Disconnect, &Param);
};


static bool TcpSslEventProc(TcpLayer_t* TcpLayer, int32 Id, const void* Param)
{
    TcpSsl_t* TcpSsl = (TcpSsl_t*)TcpLayer;
    bool bResult = false;

    switch (Id)
    {
    case TcpLayerEvent_Connect:
        {
            //
            //  Catch up connect event and hold it until ssl/tls handhake will not complete or fail
            //
            TcpLayerEventParamConnect_t* EventParam = (TcpLayerEventParamConnect_t*)Param;

			if (EventParam->FlagRemote)
				TcpSslCtxCreate(TcpSsl);

            //
            //  Buffer up transport endpoint address for notify user later
            //  when ssl/tls handshake complete
            //
            NetAddrInit(&TcpSsl->NetAddr, EventParam->Ip, EventParam->Port);

            //
            //  Initiate handshake depending on side
            //
            bResult = TcpSslHandshakeInitiate(TcpSsl);
        }
        break;

    case TcpLayerEvent_ConnectFail:
        {
            bResult = TcpLayerSendEvent(TcpLayer, Id, Param);
        }
        break;

    case TcpLayerEvent_Read:
        {
            TcpLayerEventParamRead_t* EventParam = (TcpLayerEventParamRead_t*)Param;
            bResult = TcpSslRead(TcpSsl, EventParam->Data, EventParam->DataSize);
        }
        break;

    case TcpLayerEvent_Disconnect:
        {
            TcpLayerEventParamDisconnect_t* EventParam = (TcpLayerEventParamDisconnect_t*)Param;

            if (TcpSsl->FlagHandshakeComplete)
            {
                //
                //  Dispatch disconnect event as usual
                //
                bResult = TcpLayerSendEvent(TcpLayer, Id, Param);
            }
            else
            {
                //
                //  We got dc but handshake is not completed
                //  Morph disconnect event to connect fail event
                //
                TcpLayerEventParamConnectFail_t FailParam = {};
                FailParam.Ip = NetAddrIp(&TcpSsl->NetAddr);
                FailParam.Port = NetAddrPort(&TcpSsl->NetAddr);
                FailParam.Label = TcpLayerGetLabel(TcpLayer);
                FailParam.ErrorCode = 0;

                bResult = TcpLayerSendEvent(TcpLayer, TcpLayerEvent_ConnectFail, &FailParam);
            };
        }
        break;
    };

    return bResult;
};


static bool TcpSslCmdProc(TcpLayer_t* TcpLayer, int32 Id, void* Param)
{
    TcpSsl_t* TcpSsl = (TcpSsl_t*)TcpLayer;
    bool bResult = false;

    switch (Id)
    {
    case TcpLayerCmd_Connect:
        {
            TcpLayerCmdParamConnect_t* CmdParam = (TcpLayerCmdParamConnect_t*)Param;

            if (!CmdParam->FlagCancel)
            {
                TcpSslStartupPerConnect(TcpSsl);
                bResult = TcpLayerSendCmd(TcpLayer, Id, Param);
            }
            else
            {
                bResult = TcpLayerSendCmd(TcpLayer, Id, Param);
            };
        }
        break;

    case TcpLayerCmd_Write:
        {
            TcpLayerCmdParamWrite_t* CmdParam = (TcpLayerCmdParamWrite_t*)Param;
            int32 Written = 0;
            int32 Size = CmdParam->DataSize;
            const char* Buffer = CmdParam->Data;
            bool FlagDisconnectOnComplete = false;
            
            while (Size)
            {
                int32 Write = Min(Size, TCPSSL_BUFFSIZE_WRITE);

                //
                //  Apply flag only to a last chunk
                //
                if (!(Size - Write))
                    FlagDisconnectOnComplete = CmdParam->FlagDisconnectOnComplete;

                if (!TcpSslWrite(TcpSsl, &Buffer[Written], Write, FlagDisconnectOnComplete))
                {
                    bResult = false;
                    break;
                }
                else
                {
                    bResult = true;
                    Written += Write;
                    Size -= Write;
                    ASSERT(Size >= 0);
                };
            };
        }
        break;

    case TcpSslCmd_SetHostname:
        {
            std::strcpy(TcpSsl->Hostname, (char*)Param);
            bResult = true;
        }
        break;

    default:
        {
            bResult = TcpLayerSendCmd(TcpLayer, Id, Param);
        }
        break;
    };

    return bResult;
};


struct TcpLayer_t* TcpSslCreate(void* Param)
{
    TcpSsl_t* TcpSsl = new TcpSsl_t();
    if (!TcpSsl)
        return 0;

    TcpSslStartupPerSession(TcpSsl);

    return &TcpSsl->Layer;
};


void TcpSslDestroy(struct TcpLayer_t* TcpLayer)
{
    TcpSsl_t* TcpSsl = (TcpSsl_t*)TcpLayer;

    TcpSslCleanupPerSession(TcpSsl);

    delete TcpSsl;
};