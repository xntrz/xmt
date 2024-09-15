#pragma once

#define OPENSSL_NO_SOCK
#define OPENSSL_NO_STDIO
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "openssl/x509v3.h"


class CNetSsl
{
public:

    enum TLSVER
    {
        TLSVER_11 = 0,
        TLSVER_12,

        TLSVERNUM,
    };

    enum DTLSVER
    {
        DTLSVER_12 = 0,

        DTLSVERNUM,
    };

    enum CTXTYPE
    {
        CTXTYPE_TLS11,
        CTXTYPE_TLS12,
        
        CTXTYPENUM,
    };
    
public:
    static bool Initialize(CTXTYPE CtxType);
    static void Terminate(void);
    static bool IsInitialized(void);
    static bool IsCertLoaded();
    static int32 GetError(SSL* Ssl, int32 Result);
    static bool IsFatalError(int32 SslError);
    static bool IsEof(int32 SslError);
    static SSL* Alloc(void);
    static void Free(SSL* Ssl);
    static void Shutdown(SSL* Ssl);
    static void ThreadCleanup(void);
};