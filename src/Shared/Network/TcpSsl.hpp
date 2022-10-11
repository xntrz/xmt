#pragma once

enum TcpSslCmd_t
{
    TcpSslCmd_SetHostname = 100,    // Param is a pointer to null-terminated string
};

struct TcpLayer_t* TcpSslCreate(void* Param);
void TcpSslDestroy(struct TcpLayer_t* TcpLayer);