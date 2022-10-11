#pragma once

#include "openssl/ssl.h"
#include "openssl/err.h"
#include "openssl/x509v3.h"


enum NetSslCtxType_t
{
    NetSslCtxType_tls11 = 0,
    NetSslCtxType_tls12,
    NetSslCtxTypeNum,
};


bool NetSslInitialize(NetSslCtxType_t CtxType);
void NetSslTerminate(void);
int32 NetSslGetError(SSL* Ssl, int32 Result);
bool NetSslIsFatalError(int32 SslError);
bool NetSslIsEOF(int32 SslError);
SSL* NetSslAlloc(void);
void NetSslFree(SSL* Ssl);
void NetSslShutdown(SSL* Ssl);
void NetSslThreadCleanup(void);