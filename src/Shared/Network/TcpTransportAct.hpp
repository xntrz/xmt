#pragma once

#include "NetTypedefs.hpp"


enum TcpActType_t
{
    TcpActType_Accept = 0,
    TcpActType_Connect,
    TcpActType_Read,
    TcpActType_Write,
    TcpActType_Disconnect,
};


struct TcpActTmoutNodeTag_t
{
    ;
};


struct TcpAct_t
{
    OVERLAPPED Ovl;
    TcpActType_t Type;
    void* Transport;
    volatile SOCKET hSocket;
    CListNode<TcpAct_t> TimeoutNode;
    uint32 Timeout;
    uint32 TimeStart;
    uint32 TimeoutFlags;
    bool FlagTimeoutProc;
};


struct TcpActAccept_t : public TcpAct_t
{
    SOCKET hRemoteSocket;
    char RemoteAddrBuffer[(sizeof(struct sockaddr_in) + 16) * 2];
};


struct TcpActConnect_t : public TcpAct_t
{
    ;
};


struct TcpActRead_t : public TcpAct_t
{
    ;
};


enum TcpActWriteStatus_t
{
    TcpActWriteStatus_Await = 0,
    TcpActWriteStatus_Process,
    TcpActWriteStatus_Complete,
};


struct TcpActWrite_t : public TcpAct_t
{
    CListNode<TcpActWrite_t> WriteNode;
    TcpActWriteStatus_t Status;
    char Buffer[4096];
    int32 Size;
    int32 Generation;
    bool FlagDisconnectOnComplete;
};


struct TcpActDisconnect_t : public TcpAct_t
{
    ;
};


void TcpActInitialize(void);
void TcpActTerminate(void);
TcpActWrite_t* TcpActWriteAlloc(void);
void TcpActWriteFree(TcpActWrite_t* TcpActWrite);