#include "TcpLayer.hpp"
#include "TcpSession.hpp"


HOBJ TcpLayerGetSession(TcpLayer_t* TcpLayer)
{
    return TcpLayer->hSession;
};


int32 TcpLayerGetLabel(TcpLayer_t* TcpLayer)
{
    return TcpLayer->Label;
};


bool TcpLayerIsRemote(TcpLayer_t* TcpLayer)
{
    return TcpSessionIsRemote(TcpLayer->hSession);
};


bool TcpLayerSendEvent(TcpLayer_t* TcpLayer, int32 Id, const void* Param)
{
    return TcpSessionSendEvent(TcpLayer->hSession, TcpLayer, Id, Param);
};


bool TcpLayerSendCmd(TcpLayer_t* TcpLayer, int32 Id, void* Param)
{
    return TcpSessionSendCmd(TcpLayer->hSession, TcpLayer, Id, Param);
};


void TcpLayerRefInc(TcpLayer_t* TcpLayer)
{
    TcpSessionRefInc(TcpLayer->hSession);
};


void TcpLayerRefDec(TcpLayer_t* TcpLayer)
{
    TcpSessionRefDec(TcpLayer->hSession);
};