#include "debug.hpp"

#include "Common/Time.hpp"
#include "Common/Thread.hpp"


struct DbgModule_t
{
    const char* Label;
    HINSTANCE hInstance;
    uint32 ModuleSize;
};


struct DbgContainer_t
{
    FILE* StdIn;
    FILE* StdOut;
    FILE* StdErr;
    HWND hWndConsole;
    CRITICAL_SECTION Cs;
    DbgModule_t aModule[MODULE_MAX];
    bool FlagAssertion;
};


static DbgContainer_t DbgContainer;
static const char* MODULE_LABEL_UNKNOWN = "UnkMod";
static const HINSTANCE MODULE_HANDLE_UNKNOWN = HINSTANCE(-1);


static const DbgModule_t* DbgSearchModuleByAddress(void* Address)
{
    for (int32 i = 0; i < COUNT_OF(DbgContainer.aModule); ++i)
    {
        DbgModule_t* DbgModule = &DbgContainer.aModule[i];

        if (DbgModule->hInstance != NULL)
        {
            if ((uint32(Address) >= uint32(DbgModule->hInstance)) &&
                (uint32(Address) <= (uint32(DbgModule->hInstance) + DbgModule->ModuleSize)))
                return DbgModule;
        };        
    };

    return nullptr;
};


static void DbgOutputCommon(void* CallerAddress, bool ln, const char* fname, int32 fline, const char* format, va_list& vl)
{
	char szOutputBuffer[4096 * 32];
    szOutputBuffer[0] = '\0';

    uint32 Hour = 0;
    uint32 Minute = 0;
    uint32 Second = 0;
    uint32 Ms = 0;
    TimeCurrentLocal(&Hour, &Minute, &Second, &Ms);

    const char* pszModuleLabel = MODULE_LABEL_UNKNOWN;
    const DbgModule_t* DbgModule = DbgSearchModuleByAddress(CallerAddress);
    if (DbgModule)
        pszModuleLabel = DbgModule->Label;

    int32 offset = std::sprintf(
        szOutputBuffer,
        "[%02d:%02d:%02d.%03.0f][%s]: %s.%d -- ",
        Hour, Minute, Second, float(Ms), pszModuleLabel, fname, fline
    );

    int32 Result = std::vsprintf(&szOutputBuffer[offset], format, vl);

    if (ln)
    {
        if (Result > 0)
        {
            offset += Result;
            std::sprintf(&szOutputBuffer[offset], "\n");
        };
    };

    EnterCriticalSection(&DbgContainer.Cs);
    std::printf("%s", szOutputBuffer);
	LeaveCriticalSection(&DbgContainer.Cs);
    OutputDebugStringA(szOutputBuffer);
};


static void DbgFatalNoRet(const char* reason)
{
    char szFatalBuffer[4096];
    szFatalBuffer[0] = '\0';
    std::sprintf(szFatalBuffer, "%s\n\n", reason);

    uint32 Flags = MB_ICONERROR;
    
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
	EnterCriticalSection(&DbgContainer.Cs);
    int32 iResult = MessageBoxA(NULL, szFatalBuffer, "Fatal error", Flags);
    if (iResult == IDOK)
        __debugbreak();
	LeaveCriticalSection(&DbgContainer.Cs);
    TerminateProcess(GetCurrentProcess(), -1);
};


void DbgInitialize(void)
{
    AllocConsole();
    AttachConsole(GetCurrentProcessId());

    freopen_s(&DbgContainer.StdIn, "CON", "r", stdin);
    freopen_s(&DbgContainer.StdOut, "CON", "w", stdout);
    freopen_s(&DbgContainer.StdErr, "CON", "w", stderr);

    DbgContainer.hWndConsole = GetConsoleWindow();

    InitializeCriticalSection(&DbgContainer.Cs);
};


void DbgTerminate(void)
{
    DeleteCriticalSection(&DbgContainer.Cs);

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


void DbgRegistModule(const char* pszLabel, HINSTANCE hInstance)
{
    for (int32 i = 0; i < COUNT_OF(DbgContainer.aModule); ++i)
    {
        DbgModule_t* DbgModule = &DbgContainer.aModule[i];
        if (DbgModule->hInstance == NULL)
        {
            DbgModule->Label = pszLabel;
            DbgModule->hInstance = hInstance;

            MODULEINFO ModuleInfo = {};
            if (GetModuleInformation(GetCurrentProcess(), hInstance, &ModuleInfo, sizeof(ModuleInfo)))
                DbgModule->ModuleSize = ModuleInfo.SizeOfImage;
            else
                ASSERT(false, "%u", GetLastError());

            return;
        };
    };

    ASSERT(false);
};


void DbgRemoveModule(HINSTANCE hInstance)
{
    for (int32 i = 0; i < COUNT_OF(DbgContainer.aModule); ++i)
    {
        DbgModule_t* DbgModule = &DbgContainer.aModule[i];
        if (DbgModule->hInstance == hInstance)
        {
            DbgModule->hInstance = NULL;
            return;
        };
    };

    ASSERT(false);
};


void DbgAssert(const char* expression, const char* fname, int32 fline)
{
#ifdef _DEBUG
    EnterCriticalSection(&DbgContainer.Cs);
    DbgAssert(expression, fname, fline, "Unknown error\n");
    LeaveCriticalSection(&DbgContainer.Cs);
#endif
};


void DbgAssert(const char* expression, const char* fname, int32 fline, const char* format, ...)
{
#ifdef _DEBUG
    EnterCriticalSection(&DbgContainer.Cs);
    DbgContainer.FlagAssertion = true;
    ThreadSuspendAllExceptThis();

    static char buff[4096] = { 0 };
    
    HMODULE ModuleHandle = MODULE_HANDLE_UNKNOWN;
    const char* ModuleLabel = MODULE_LABEL_UNKNOWN;
    const DbgModule_t* DbgModule = DbgSearchModuleByAddress(_ReturnAddress());
    if (DbgModule)
    {
        ModuleHandle = DbgModule->hInstance;
        ModuleLabel = DbgModule->Label;
    };

    int32 written = sprintf_s(buff, sizeof(buff),
        "Assertion!\n"
        "\n"
        "Expression: %s\n"
        "File: %s(%u)\n"
        "Thread: %s(0x%x)\n"
        "Module: %s(0x%x)\n"
        "Description:\n",
        expression,
        fname, fline,
        ThreadGetName(), ThreadGetId(),
        ModuleLabel, uint32(ModuleHandle)
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
    LeaveCriticalSection(&DbgContainer.Cs);
    ThreadResumeAllExceptThis();
#endif
};


void DbgOutput(const char* fname, int32 fline, const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    DbgOutputCommon(_ReturnAddress(), false, fname, fline, format, vl);
    va_end(vl);
};


void DbgOutputLn(const char* fname, int32 fline, const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    DbgOutputCommon(_ReturnAddress(), true, fname, fline, format, vl);
    va_end(vl);
};


void DbgFatal(const char* reason, ...)
{
    char Buff[4096];
    Buff[0] = '\0';
    
    va_list vl;
    va_start(vl, reason);
    std::vsprintf(Buff, reason, vl);
    DbgFatalNoRet(Buff);
    va_end(vl);
};