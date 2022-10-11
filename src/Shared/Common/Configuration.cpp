#include "Configuration.hpp"
#include "Ini.hpp"
#include "Registry.hpp"

#include "Shared/File/File.hpp"
#include "Shared/File/FsRc.hpp"


#define CFG_NAME_MAX (64)


struct CfgLoadInfo_t
{
    HMODULE hModule;
    int32 DefaultRcId;
    char Label[CFG_NAME_MAX];
};


struct CfgContainer_t
{
    char AppNameA[CFG_NAME_MAX];
    wchar_t AppNameW[CFG_NAME_MAX];
    HINSTANCE hAppInstance;
    CfgLoadInfo_t LoadInfo[MODULE_MAX];
    int32 LoadInfoNum;
    std::atomic<bool> FlagReset;
};


static CfgContainer_t CfgContainer;


static void CfgReadIniData(const char* Buffer, uint32 BufferSize, bool bIsDefault)
{
    HOBJ hIni = IniOpen(Buffer, BufferSize);
    if (!hIni)
        return;

    auto FnEnumIniKey = [](const char* Segment, const char* Key, const char* Value, void* Param) -> bool
    {
        HOBJ hVar = RegVarFind(Key);
        if (hVar)
        {
			bool bIsDefault = (Param != nullptr);
            if (!bIsDefault)
            {
                OUTPUTLN(
                    "Overriding '%s' with value: '%s' (old: '%s')",
                    Key,
                    Value,
                    RegVarReadString(hVar)
                );

                RegVarSetValue(hVar, Value);
            };
        }
        else
        {
            OUTPUTLN(
                "Registering '%s' with value: '%s'",
                Key,
                Value
            );
            
            hVar = RegVarRegist(Key);
            if (hVar)
            {
                RegVarSetValue(hVar, Value);
            };
        };
        
        return true;
    };

    IniKeyEnum(hIni, FnEnumIniKey, (void*)bIsDefault);
    IniClose(hIni);
    hIni = 0;
};


static void CfgReadIniFile(HOBJ hFile, bool bIsDefault)
{
    uint32 FSize = uint32(FileSize(hFile));
    if (!FSize)
        return;

    char* FBuff = new char[FSize];
    ASSERT(FBuff);
    if (FBuff)
    {
        uint32 Readed = FileRead(hFile, FBuff, FSize);
        ASSERT(Readed == FSize);
        if (Readed == FSize)
            CfgReadIniData(FBuff, FSize, bIsDefault);
        
        delete[] FBuff;
        FBuff = nullptr;
    };
};


static bool CfgWinverGreaterOrEqual(WORD Winver)
{
    OSVERSIONINFOEX OsVerInfo = { 0 };
    DWORDLONG ConditionMask = 0;
    int32 Op = VER_GREATER_EQUAL;

    OsVerInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    OsVerInfo.dwMajorVersion = HIBYTE(Winver);
    OsVerInfo.dwMinorVersion = LOBYTE(Winver);

    VER_SET_CONDITION(ConditionMask, VER_MAJORVERSION, Op);
    VER_SET_CONDITION(ConditionMask, VER_MINORVERSION, Op);

    BOOL bResult = VerifyVersionInfo(&OsVerInfo, VER_MAJORVERSION | VER_MINORVERSION, ConditionMask);
    return bool(bResult > 0);
};


bool CfgCheckWinver(void)
{
    return CfgWinverGreaterOrEqual(_WIN32_WINNT_WIN7);
};


void CfgInitialize(const char* AppName, HINSTANCE hInstance)
{
    TCHAR	szModulePath[MAX_PATH] = { 0 };
	TCHAR*	pszResult = nullptr;

	GetModuleFileName(NULL, szModulePath, sizeof(szModulePath));
	pszResult = _tcsrchr(szModulePath, TEXT('\\'));
	*pszResult = TEXT('\0');
	SetCurrentDirectory(szModulePath);

    RegRefInc();

    ASSERT(std::strlen(AppName) < COUNT_OF(CfgContainer.AppNameA));    
    std::strcpy(CfgContainer.AppNameA, AppName);
    std::string StrAppName(AppName);
    std::wcscpy(CfgContainer.AppNameW, std::wstring(StrAppName.begin(), StrAppName.end()).c_str());
    CfgContainer.hAppInstance = hInstance;
    CfgContainer.FlagReset = false;
};


void CfgTerminate(void)
{
    RegRefDec();
};


bool CfgIsArgPresent(const char* pszArg)
{
	std::string str0(pszArg);
	std::tstring str1(str0.begin(), str0.end());

	TCHAR** argv = __targv;
	int32 argc = __argc;

    for (int32 i = 0; i < argc; ++i)
    {
        const TCHAR* arg = argv[i];

        if ((arg[0] == TEXT('-')) && arg[1])        
		{
            if (!_tcscmp(&arg[1], &str1[0]))
                return true;
        };
    };

    return false;
};


const char* CfgGetCurrentDirA(void)
{
    thread_local static char szCurrentDirBuff[MAX_PATH];
    GetCurrentDirectoryA(COUNT_OF(szCurrentDirBuff), szCurrentDirBuff);
    return szCurrentDirBuff;
};


const wchar_t* CfgGetCurrentDirW(void)
{
    thread_local static wchar_t wszCurrentDirBuff[MAX_PATH];
    GetCurrentDirectoryW(COUNT_OF(wszCurrentDirBuff), wszCurrentDirBuff);
    return wszCurrentDirBuff;
};


const char* CfgGetAppNameA(void)
{
    return CfgContainer.AppNameA;
};


const wchar_t* CfgGetAppNameW(void)
{
    return CfgContainer.AppNameW;
};


HINSTANCE CfgGetAppInstance(void)
{
    return CfgContainer.hAppInstance;
};


void CfgLoad(HMODULE hModule, const char* Label, int32 DefaultRcId)
{
    //
    //  Ignore registering new module for loading if we are executing reset
    //
    if (!CfgContainer.FlagReset)
    {
        if (CfgContainer.LoadInfoNum >= COUNT_OF(CfgContainer.LoadInfo))
            return;

        for (int32 i = 0; i < CfgContainer.LoadInfoNum; ++i)
        {
            if (CfgContainer.LoadInfo[i].hModule == hModule)
                return;
        };

        int32 Index = CfgContainer.LoadInfoNum++;
        CfgContainer.LoadInfo[Index].hModule = hModule;
        CfgContainer.LoadInfo[Index].DefaultRcId = DefaultRcId;
        
        ASSERT(std::strlen(Label) < COUNT_OF(CfgLoadInfo_t::Label));
        std::strcpy(CfgContainer.LoadInfo[Index].Label, Label);
    };
    
    if (DefaultRcId != -1)
    {
        HOBJ hRcFile = FileOpen(RcFsBuildPath(hModule, DefaultRcId), "rb");
        if (hRcFile)
        {
            CfgReadIniFile(hRcFile, true);
            FileClose(hRcFile);
        };
    };

    std::string PhyFilepath;
    PhyFilepath += Label;
    PhyFilepath += ".ini";

    HOBJ hPhyFile = FileOpen(&PhyFilepath[0], "rb");
    if (hPhyFile)
    {
        CfgReadIniFile(hPhyFile, false);
        FileClose(hPhyFile);
    }
	else
	{
		OUTPUTLN("failed to open %s cfg file", Label);
	};
};


void CfgSave(void)
{
    char* FBuff = nullptr;
    uint32 FSize = 0;
    
    HOBJ hIni = IniNew();
    if (hIni)
    {
        auto fnRegEnumCallback = [](HOBJ hVar, const char* Name, const char* Value, void* Param) -> void
        {
            HOBJ hIni = HOBJ(Param);
            IniKeyNew(hIni, nullptr, Name);
            IniKeyWriteString(hIni, nullptr, Name, Value);
        };
        
        RegVarEnum(fnRegEnumCallback, hIni);

        if (IniSave(hIni, FBuff, (int32*)&FSize))
        {
            FBuff = new char[FSize];
            IniSave(hIni, FBuff, (int32*)&FSize);
        };

        IniClose(hIni);
    };

    if (FBuff)
    {
        std::string Filepath;
        Filepath += CfgContainer.AppNameA;
        Filepath += ".ini";

        HOBJ hPhyFile = FileOpen(&Filepath[0], "wb");
        if (hPhyFile)
        {
            FileWrite(hPhyFile, FBuff, FSize);
            FileClose(hPhyFile);
        };

        if (FBuff)
        {
            delete[] FBuff;
            FBuff = nullptr;
        };
    };    
};


void CfgReset(void)
{
    if (CfgContainer.FlagReset)
        return;

    CfgContainer.FlagReset = true;

    RegRefDec();
    RegRefInc();

    for (int32 i = CfgContainer.LoadInfoNum; i > 0; --i)
    {
        int32 Index = (i - 1);        
        CfgLoad(
            CfgContainer.LoadInfo[Index].hModule,
            CfgContainer.LoadInfo[Index].Label,
            CfgContainer.LoadInfo[Index].DefaultRcId
        );
    };

    RegResetExec();

    CfgContainer.FlagReset = false;
};


bool CfgIsTestMode(void)
{
    return (GetAsyncKeyState(VK_HOME) & 0x8000) == 0x8000;
};