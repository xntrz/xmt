#include "Configuration.hpp"
#include "Ini.hpp"
#include "Registry.hpp"

#include "Shared/File/File.hpp"
#include "Shared/File/FsRc.hpp"

#include "shellapi.h"


struct CCfgContainer
{
    CCfgContainer(const std::string& appname, HINSTANCE hInstance);
	~CCfgContainer();
    void save();
    void load(HMODULE hMod, const std::string& modname, int32 rcid = -1);
    void read_ini(HOBJ hFile, bool bDefault);
    void read_ini_mem(const char* buff, std::size_t size, bool bDefault);
    void setup_dir();
    
    inline char** argv() const { return m_argv; };
	inline int argc() const { return m_argc; };
	
	inline HINSTANCE instance() const { return m_hInstance; };
    inline const std::string& appname() const { return m_appname; };

private:
	std::string m_appname;
    HINSTANCE m_hInstance;
	char** m_argv;
	int m_argc;
};


CCfgContainer::CCfgContainer(const std::string& appname, HINSTANCE hInstance)
: m_appname(appname)
, m_hInstance(hInstance)
, m_argv(nullptr)
, m_argc(0)
{
	LPWSTR* pwszArgv = CommandLineToArgvW(GetCommandLineW(), &m_argc);
	m_argv = new char*[m_argc];

	for (int i = 0; i < m_argc; ++i)
	{
		int size = std::wcslen(pwszArgv[i]) + 1;
		m_argv[i] = new char[size];
		std::wcstombs(m_argv[i], pwszArgv[i], size);
	};

	LocalFree(pwszArgv);
	pwszArgv = NULL;
};


CCfgContainer::~CCfgContainer()
{
	if (m_argv)
	{
		for (int i = 0; i < m_argc; ++i)
		{
			ASSERT(m_argv[i] != nullptr);

			delete[] m_argv[i];
			m_argv[i] = nullptr;
		};

		delete[] m_argv;
		m_argv = nullptr;

		m_argc = 0;
	};	
};


void CCfgContainer::save()
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
        std::string Filepath(m_appname + ".ini");

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


void CCfgContainer::load(HMODULE hMod, const std::string& modname, int32 rcid /*= -1*/)
{
    if (rcid != -1)
    {
        HOBJ hRcFile = FileOpen(RcFsBuildPath(hMod, rcid), "rb");
        if (hRcFile)
        {
            read_ini(hRcFile, true);
            FileClose(hRcFile);
        };
    };

    std::string PhyFilepath(modname + ".ini");

    HOBJ hPhyFile = FileOpen(&PhyFilepath[0], "rb");
    if (hPhyFile)
    {
        read_ini(hPhyFile, false);
        FileClose(hPhyFile);
    }
    else
    {
        OUTPUTLN("failed to open %s cfg file", modname.c_str());
    };
};


void CCfgContainer::read_ini(HOBJ hFile, bool bDefault)
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
            read_ini_mem(FBuff, FSize, bDefault);

        delete[] FBuff;
        FBuff = nullptr;
    };
};


void CCfgContainer::read_ini_mem(const char* buff, std::size_t size, bool bDefault)
{
    HOBJ hIni = IniOpen(buff, size);
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
                OUTPUTLN("Overriding '%s' with value: '%s' (old: '%s')", Key, Value, RegVarReadString(hVar));
                RegVarSetValue(hVar, Value);
            };
        }
        else
        {
            OUTPUTLN("Registering '%s' with value: '%s'", Key, Value);

            hVar = RegVarRegist(Key);
            if (hVar)
            {
                RegVarSetValue(hVar, Value);
            };
        };

        return true;
    };

    IniKeyEnum(hIni, FnEnumIniKey, (void*)bDefault);
    IniClose(hIni);
    hIni = 0;
};


void CCfgContainer::setup_dir()
{
    char	szModulePath[MAX_PATH] = { 0 };
    char* pszResult = nullptr;

    GetModuleFileNameA(NULL, szModulePath, sizeof(szModulePath));
    pszResult = std::strrchr(szModulePath, '\\');
    *pszResult = '\0';
    SetCurrentDirectoryA(szModulePath);
};


static std::unique_ptr<CCfgContainer> s_pCfgContainer = nullptr;


void CfgInitialize(const std::string& appname, HINSTANCE hInstance)
{
    RegRefInc();

    s_pCfgContainer = std::make_unique<CCfgContainer>(appname, hInstance);
    s_pCfgContainer->setup_dir();
};


void CfgTerminate(void)
{
    s_pCfgContainer = nullptr;

    RegRefDec();
};


/*DLLSHARED*/ bool CfgIsArgPresent(const char* pszArg)
{
	char** argv = s_pCfgContainer->argv();
	int argc = s_pCfgContainer->argc();

    for (int i = 0; i < argc; ++i)
    {
        const char* arg = argv[i];

        if ((arg[0] == TEXT('-')) && arg[1])
        {
            if(std::strcmp(&arg[1], pszArg) == 0)
                return true;
        };
    };

    return false;
};


/*DLLSHARED*/ const char* CfgGetCurrentDir(void)
{
    thread_local static char szCurrentDirBuff[MAX_PATH];
    GetCurrentDirectoryA(COUNT_OF(szCurrentDirBuff), szCurrentDirBuff);
    return szCurrentDirBuff;
};


/*DLLSHARED*/ HINSTANCE CfgGetAppInstance(void)
{
    return s_pCfgContainer->instance();
};


/*DLLSHARED*/ void CfgLoad(HMODULE hMod, const std::string& modname, int32 rcid /*= -1*/)
{
    s_pCfgContainer->load(hMod, modname, rcid);
};


/*DLLSHARED*/ void CfgSave(void)
{
    s_pCfgContainer->save();
};


/*DLLSHARED*/ bool MakeWindowScreenshot(HWND hWnd)
{
	bool bResult = false;

	RECT rcWnd;
	GetClientRect(hWnd, &rcWnd);

	HDC hDCScr = GetDC(NULL);
	if (hDCScr != NULL)
	{
		HDC hDC = CreateCompatibleDC(hDCScr);
		if (hDC != NULL)
		{
			HBITMAP hBitmap = CreateCompatibleBitmap(hDCScr, rcWnd.right - rcWnd.left, rcWnd.bottom - rcWnd.top);
			if (hBitmap != NULL)
			{
				SelectObject(hDC, hBitmap);
				PrintWindow(hWnd, hDC, 0);

				OpenClipboard(NULL);
				EmptyClipboard();
				SetClipboardData(CF_BITMAP, hBitmap);
				CloseClipboard();

				bResult = true;

				DeleteObject(hBitmap);
			};

			DeleteDC(hDC);
		};

		ReleaseDC(NULL, hDCScr);
	};

	return bResult;
};