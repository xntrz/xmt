#include "debug.hpp"

#include "Common/Time.hpp"
#include "Common/Thread.hpp"

#include <intrin.h>


#define DbgInitReturnAddress() \
    (DbgReturnAddress = (DbgReturnAddress ? DbgReturnAddress : _ReturnAddress()), DbgReturnAddress)

#define DbgClearReturnAddress() \
    (DbgReturnAddress = nullptr)


struct DbgContainerModule_t
{
    HMODULE base;
    char label[64];
};


struct DbgContainer_t
{
    FILE* StdIn;
    FILE* StdOut;
    FILE* StdErr;
    HWND hWndConsole;
    std::recursive_mutex Mutex;
    DbgContainerModule_t Modules[32];
    bool FlagAssertion;
};


static DbgContainer_t DbgContainer;
thread_local void* DbgReturnAddress = nullptr;


static HMODULE DbgGetModuleBaseByAddress(void* Address)
{
    HMODULE hModule = NULL;
    DWORD dwFlags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;

    GetModuleHandleEx(dwFlags, LPTSTR(Address), &hModule);

    return hModule;
};


static const char* DbgGetModuleNameByAddress(void* Address)
{
    HMODULE hModule = DbgGetModuleBaseByAddress(Address);
    if (hModule != NULL)
    {
        for (int32 i = 0; i < COUNT_OF(DbgContainer.Modules); ++i)
        {
            if (DbgContainer.Modules[i].base == hModule)
                return DbgContainer.Modules[i].label;
        };
    };

    return "UNKNOWN_MODULE_NAME";
};


static void DbgOutputCommon(bool ln, const char* fname, int32 fline, const char* format, va_list& vl)
{
	char szOutputBuffer[4096 * 4];
    szOutputBuffer[0] = '\0';

    uint32 Hour = 0;
    uint32 Minute = 0;
    uint32 Second = 0;
    uint32 Ms = 0;
    TimeCurrentLocal(&Hour, &Minute, &Second, &Ms);

    const char* mname = DbgGetModuleNameByAddress(DbgReturnAddress);

    int32 offset = std::sprintf(
        szOutputBuffer,
        "[%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32 ":%03.0f][%s]: %s.%" PRIi32 " -- ",
        Hour, Minute, Second, float(Ms), mname, fname, fline
    );

    int32 Result = std::vsprintf(&szOutputBuffer[offset], format, vl);
    if (ln && (Result > 0))
    {
        offset += Result;
        std::sprintf(&szOutputBuffer[offset], "\n");
    };

    {
        std::unique_lock<std::recursive_mutex> lock(DbgContainer.Mutex);
        std::printf("%s", szOutputBuffer);
    }

    OutputDebugStringA(szOutputBuffer);
};


static void DbgFatalNoRet(const char* reason)
{
    char szFatalBuffer[4096];
    szFatalBuffer[0] = '\0';
    std::sprintf(szFatalBuffer, "%s\n\n", reason);

    uint32 Flags = MB_ICONERROR;

#ifdef _DEBUG
    if (DbgContainer.FlagAssertion)
    {
        std::strcat(szFatalBuffer, "Press OK to execute debugbreak or CANCEL to terminate program.");
        Flags |= (MB_OKCANCEL | MB_DEFBUTTON2);
    }
    else
    {
        std::strcat(szFatalBuffer, "Press OK to terminate program.");
        Flags |= (MB_OK | MB_DEFBUTTON1);
    };
#else
	std::strcat(szFatalBuffer, "Press OK to terminate program.");
	Flags |= (MB_OK | MB_DEFBUTTON1);
#endif

    {
        std::unique_lock<std::recursive_mutex> lock(DbgContainer.Mutex);
        int32 iResult = MessageBoxA(NULL, szFatalBuffer, "Fatal error", Flags);
        if (iResult == IDOK)
            __debugbreak();
    }

    TerminateProcess(GetCurrentProcess(), UINT(-1));
};


/*DLLSHARED*/ void DbgInitialize(void)
{
    AllocConsole();
    AttachConsole(GetCurrentProcessId());

    freopen_s(&DbgContainer.StdIn, "CON", "r", stdin);
    freopen_s(&DbgContainer.StdOut, "CON", "w", stdout);
    freopen_s(&DbgContainer.StdErr, "CON", "w", stderr);

    DbgContainer.hWndConsole = GetConsoleWindow();
};


/*DLLSHARED*/ void DbgTerminate(void)
{
    std::fclose(DbgContainer.StdIn);
    std::fclose(DbgContainer.StdOut);
    std::fclose(DbgContainer.StdErr);

    FreeConsole();
    PostMessage(DbgContainer.hWndConsole, WM_CLOSE, 0, 0);

    DbgContainer.StdIn = nullptr;
    DbgContainer.StdOut = nullptr;
    DbgContainer.StdErr = nullptr;
    DbgContainer.hWndConsole = NULL;
};


/*DLLSHARED*/ void DbgRegistModule(const char* pszLabel, HINSTANCE hInstance)
{
    for (int32 i = 0; i < COUNT_OF(DbgContainer.Modules); ++i)
    {
        if (DbgContainer.Modules[i].base == 0)
        {
            DbgContainer.Modules[i].base = hInstance;

            ASSERT(std::strlen(pszLabel) < sizeof(DbgContainer.Modules[i].label));
            std::strcpy(DbgContainer.Modules[i].label, pszLabel);
        };
    };
};


/*DLLSHARED*/ void DbgRemoveModule(HINSTANCE hInstance)
{
    for (int32 i = 0; i < COUNT_OF(DbgContainer.Modules); ++i)
    {
        if (DbgContainer.Modules[i].base == hInstance)
        {
            DbgContainer.Modules[i].base = 0;
            DbgContainer.Modules[i].label[0] = '\0';
        };
    };
};


/*DLLSHARED*/ void DbgAssert(const char* expression, const char* fname, int32 fline)
{
#ifdef _DEBUG
    DbgInitReturnAddress();
    DbgAssert(expression, fname, fline, "Unknown error\n");
    DbgClearReturnAddress();
#endif
};


/*DLLSHARED*/ void DbgAssert(const char* expression, const char* fname, int32 fline, const char* format, ...)
{
#ifdef _DEBUG
    DbgContainer.FlagAssertion = true;

    char buff[4096] = { 0 };

    void* Address = DbgInitReturnAddress();
    DbgClearReturnAddress();

    HMODULE hModule = DbgGetModuleBaseByAddress(Address);
    const char* pszModuleName = DbgGetModuleNameByAddress(Address);

    int32 written = sprintf_s(buff, sizeof(buff),
        "Assertion!\n"
        "\n"
        "Expression:    %s\n"
        "File:          %s(%" PRIi32 ")\n"
        "Thread name:   %s\n"
        "Thread id:     %" PRIu32 " (0x%" PRIx32 ")\n"
        "Thread app id: %" PRIu32 " (0x%" PRIx32 ")\n"
        "Module:        %s (0x%" PRIxPTR ")\n"
        "Description:\n",
        expression,
        fname, fline,
        thread::current_name(),
        thread::current_id(), thread::current_id(),
        thread::current_app_id(), thread::current_app_id(),
        pszModuleName, hModule
    );

    if (written)
    {
        va_list vl;
        va_start(vl, format);
        written += vsprintf_s(buff + written, sizeof(buff) - written, format, vl);
        va_end(vl);
    };

    OUTPUT(buff);
    DbgFatalNoRet(buff);
    
    DbgContainer.FlagAssertion = false;
#endif
};


/*DLLSHARED*/ void DbgOutput(const char* fname, int32 fline, const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    DbgInitReturnAddress();
    DbgOutputCommon(false, fname, fline, format, vl);
    DbgClearReturnAddress();
    va_end(vl);
};


/*DLLSHARED*/ void DbgOutputLn(const char* fname, int32 fline, const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    DbgInitReturnAddress();
    DbgOutputCommon(true, fname, fline, format, vl);
    DbgClearReturnAddress();
    va_end(vl);
};


/*DLLSHARED*/ void DbgFatal(const char* reason, ...)
{
    char Buff[4096];
    Buff[0] = '\0';
    
    va_list vl;
    va_start(vl, reason);
    std::vsprintf(Buff, reason, vl);
    DbgFatalNoRet(Buff);
    va_end(vl);
};