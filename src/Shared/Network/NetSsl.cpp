#include "NetSsl.hpp"
//#include "NetSettings.hpp"

#include "Shared/File/File.hpp"
#include "Shared/Common/Mem.hpp"


struct CRYPTO_dynlock_value
{
    std::mutex Mutex;
};


struct SslContainer_t
{
    SSL_CTX* Ctx;
    std::atomic<int32> ObjRef;
	std::atomic<int32> LockRef;
	std::atomic<int32> LockNum;
    CRYPTO_dynlock_value* LockArray;
    struct
    {
        void*(*Alloc)(size_t, const char*, int);
        void*(*Realloc)(void*, size_t, const char*, int);
        void(*Free)(void*, const char*, int);
    } DefMemFunctions;
    bool CertLoaded;
};


static SslContainer_t SslContainer;


static CRYPTO_dynlock_value* NetSslDynlock_CreateCallback(const char* file, int32 line);
static void NetSslDynlock_DestroyCallback(CRYPTO_dynlock_value* lock, const char* file, int32 line);
static void NetSslDynlock_LockCallback(int32 mode, CRYPTO_dynlock_value* lock, const char* file, int32 line);
static void NetSslDynlock_LockingCallback(int32 mode, int32 no, const char* file, int32 line);


static void* NetSslMemAlloc(size_t size, const char* fname, int fline)
{
#ifdef _DEBUG
    MemSetAllocSource(fname ? fname : __FILE__, fline ? fline : __LINE__);
    return MemAlloc(size);
#else
    (void)fname;
    (void)fline;
    return MemAlloc(size);
#endif
};


static void* NetSslMemRealloc(void* ptr, size_t size, const char* fname, int fline)
{
#ifdef _DEBUG
    MemSetAllocSource(fname ? fname : __FILE__, fline ? fline : __LINE__);
    return MemRealloc(ptr, size);
#else
    (void)fname;
    (void)fline;
    return MemRealloc(ptr, size);
#endif
};


static void NetSslMemFree(void* ptr, const char* fname, int fline)
{
    (void)fname;
    (void)fline;
    MemFree(ptr);
};


static bool NetSslMemInitialize(void)
{
	CRYPTO_get_mem_functions(
        &SslContainer.DefMemFunctions.Alloc,
        &SslContainer.DefMemFunctions.Realloc,
        &SslContainer.DefMemFunctions.Free
    );

    int32 Result = CRYPTO_set_mem_functions(
        NetSslMemAlloc,
        NetSslMemRealloc,
        NetSslMemFree
    );

    ASSERT(Result > 0);
    return (Result > 0);
};


static void NetSslMemTerminate(void)
{
    CRYPTO_set_mem_functions(
        SslContainer.DefMemFunctions.Alloc,
        SslContainer.DefMemFunctions.Realloc,
        SslContainer.DefMemFunctions.Free
    );
};


static void NetSslLockInitialize(void)
{
    SslContainer.LockNum = CRYPTO_num_locks();

    if (SslContainer.LockNum)
    {
        SslContainer.LockArray = new CRYPTO_dynlock_value[SslContainer.LockNum];
        ASSERT(SslContainer.LockArray);

//#ifdef _DEBUG
//        CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);
//#endif

        CRYPTO_set_dynlock_create_callback(&NetSslDynlock_CreateCallback);
        CRYPTO_set_dynlock_destroy_callback(&NetSslDynlock_DestroyCallback);
        CRYPTO_set_locking_callback(&NetSslDynlock_LockingCallback);
        CRYPTO_set_dynlock_lock_callback(&NetSslDynlock_LockCallback);
    };
};


static void NetSslLockTerminate(void)
{
    ASSERT(SslContainer.LockRef == 0);

    if (SslContainer.LockNum)
    {
        ASSERT(SslContainer.LockArray);

        if (SslContainer.LockArray)
        {
            delete[] SslContainer.LockArray;
            SslContainer.LockArray = nullptr;
        };

        SslContainer.LockNum = 0;

        CRYPTO_set_dynlock_lock_callback(nullptr);
        CRYPTO_set_locking_callback(nullptr);
        CRYPTO_set_dynlock_destroy_callback(nullptr);
        CRYPTO_set_dynlock_create_callback(nullptr);
    };
};


static CRYPTO_dynlock_value* NetSslDynlock_CreateCallback(const char* file, int32 line)
{
    CRYPTO_dynlock_value* lock = new CRYPTO_dynlock_value;
    ASSERT(lock);

    ++SslContainer.LockRef;
    return lock;
};


static void NetSslDynlock_DestroyCallback(CRYPTO_dynlock_value* lock, const char* file, int32 line)
{
    ASSERT(SslContainer.LockRef > 0);
    --SslContainer.LockRef;

    ASSERT(lock);
    delete lock;
};


static void NetSslDynlock_LockCallback(int32 mode, CRYPTO_dynlock_value* lock, const char* file, int32 line)
{
    ASSERT(lock);

    if (mode & CRYPTO_LOCK)
        lock->Mutex.lock();
    else
        lock->Mutex.unlock();
};


static void NetSslDynlock_LockingCallback(int32 mode, int32 no, const char* file, int32 line)
{
    ASSERT(SslContainer.LockArray);
    ASSERT(SslContainer.LockNum > 0);

    if (mode & CRYPTO_LOCK)
        SslContainer.LockArray[no].Mutex.lock();
    else
        SslContainer.LockArray[no].Mutex.unlock();
};


static bool NetSslSetCertOrKeyFromMemory(char* MemBuff, int32 MemBuffLen)
{
    bool bResult = false;
    
    BIO* Bio = BIO_new_mem_buf((void*)MemBuff, MemBuffLen);
    if (Bio)
    {
		X509* SslCert = PEM_read_bio_X509(Bio, nullptr, nullptr, nullptr);
        if (SslCert)
        {
            if (SSL_CTX_use_certificate(SslContainer.Ctx, SslCert) > 0)
                bResult = true;
            
            X509_free(SslCert);
            SslCert = nullptr;
        };

		BIO_reset(Bio);

        EVP_PKEY* SslPriKey = PEM_read_bio_PrivateKey(Bio, 0, 0, 0);
        if (SslPriKey)
        {
            if (SSL_CTX_use_PrivateKey(SslContainer.Ctx, SslPriKey) > 0)
                bResult = true;
            
            EVP_PKEY_free(SslPriKey);
            SslPriKey = nullptr;
        };

        BIO_free(Bio);
        Bio = nullptr;
    };

    return bResult;
};


static bool NetSslReadCertOrKey(const char* Path)
{
    bool bResult = false;

    HOBJ hPhyFile = FileOpen(Path, "rb");
    if (hPhyFile)
    {
        uint32 FSize = uint32(FileSize(hPhyFile));
        char* FBuff = new char[FSize];
        if (FBuff)
        {
            if (FileRead(hPhyFile, FBuff, FSize) == FSize)
                bResult = NetSslSetCertOrKeyFromMemory(FBuff, FSize);

            delete[] FBuff;
            FBuff = nullptr;
        };

        FileClose(hPhyFile);
    };

    return bResult;
};


static void NetSslThreadStopCallback(HOBJ hThread)
{
    ERR_remove_state(0);
    OPENSSL_thread_stop();
};


/*static*/ bool CNetSsl::Initialize(CTXTYPE CtxType)
{
    if (!NetSslMemInitialize())
        return false;

    SSL_load_error_strings();
    
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	SSL_library_init();
#else
    OPENSSL_init_crypto(OPENSSL_INIT_NO_ATEXIT, NULL);
#endif
    
    OpenSSL_add_all_algorithms();

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
    NetSslLockInitialize();
#endif

    static const SSL_METHOD*(*const NetSslCtxMethodFn[])(void) =
    {
        TLSv1_1_method,
        TLSv1_2_method,
    };

    static_assert(COUNT_OF(NetSslCtxMethodFn) == CTXTYPENUM, "update me");

    SslContainer.Ctx = SSL_CTX_new(NetSslCtxMethodFn[CtxType]());
    ASSERT(SslContainer.Ctx);
    if (!SslContainer.Ctx)
        return false;

#ifdef _DEBUG    
    //
    //  Generate test key/cert pair:
    //  openssl req -x509 -sha256 -nodes -days 365 -newkey rsa:2048 -keyout privateKey.key -out certificate.crt
    //
	//if (NetSettings.SslPathCert[0] != '\0')
	//{
	//	if (!NetSslReadCertOrKey(NetSettings.SslPathCert))
	//		return false;
	//};
	//
	//if (NetSettings.SslPathKey[0] != '\0')
	//{
	//	if (!NetSslReadCertOrKey(NetSettings.SslPathKey))
	//		return false;
    //};
    //
    // SslContainer.CertLoaded = true;
#endif    

    return true;
};


/*static*/ void CNetSsl::Terminate(void)
{
    ASSERT(SslContainer.ObjRef == 0);

    if (SslContainer.Ctx)
    {
        SSL_CTX_free(SslContainer.Ctx);
        SslContainer.Ctx = nullptr;
    };

    CRYPTO_cleanup_all_ex_data();
    FIPS_mode_set(0);
    CONF_modules_unload(1);
    ERR_remove_state(0);
    ERR_free_strings();
    ERR_remove_thread_state(nullptr);
    EVP_cleanup();
    OPENSSL_cleanup();

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
    NetSslLockTerminate();
#endif

    NetSslMemTerminate();
};


/*static*/ bool CNetSsl::IsInitialized(void)
{
    return true;
};


/*static*/ bool CNetSsl::IsCertLoaded()
{
	return SslContainer.CertLoaded;
};


/*static*/ int32 CNetSsl::GetError(SSL* Ssl, int32 Result)
{
    int32 Error = SSL_get_error(Ssl, Result);
    if (SSL_ERROR_NONE != Error)
    {
        char Message[1024] = { 0 };
        int32 ErrorCode = Error;
        while (ErrorCode != SSL_ERROR_NONE)
        {
            ERR_error_string_n(ErrorCode, Message, COUNT_OF(Message));
            if (CNetSsl::IsFatalError(ErrorCode))
                OUTPUTLN("SSL ERROR %" PRIi32 " - %s", ErrorCode, Message);

            ErrorCode = ERR_get_error();
        };
    };
    
    return Error;
};


/*static*/ bool CNetSsl::IsFatalError(int32 SslError)
{
    switch (SslError)
    {
    case SSL_ERROR_NONE:
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
    case SSL_ERROR_WANT_CONNECT:
    case SSL_ERROR_WANT_ACCEPT:
    case SSL_ERROR_ZERO_RETURN:
        return false;

    default:
        return true;
    };
};


/*static*/ bool CNetSsl::IsEof(int32 SslError)
{
    return (SslError == SSL_ERROR_ZERO_RETURN);
};


/*static*/ SSL* CNetSsl::Alloc(void)
{
    SSL* pSSL = SSL_new(SslContainer.Ctx);
    if (pSSL)
        ++SslContainer.ObjRef;

    return pSSL;
};


/*static*/ void CNetSsl::Free(SSL* Ssl)
{
    ASSERT(Ssl);
    SSL_free(Ssl);

    ASSERT(SslContainer.ObjRef > 0);
    --SslContainer.ObjRef;    
};


/*static*/ void CNetSsl::Shutdown(SSL* Ssl)
{
    ASSERT(Ssl);
    
    if (SSL_is_init_finished(Ssl) > 0)
    {
        int32 iResult = SSL_shutdown(Ssl);
        if (iResult < 0)
        {
            int32 iErrorCode = CNetSsl::GetError(Ssl, iResult);
        };
    };
};


/*static*/ void CNetSsl::ThreadCleanup(void)
{
    ERR_remove_state(0);
    OPENSSL_thread_stop();
};