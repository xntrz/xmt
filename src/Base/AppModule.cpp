#include "AppModule.hpp"

#include "Shared/Common/Configuration.hpp"


static const bool APP_MODULE_RECURSIVE_SRCH = false;
static const int32 APP_MODULE_TERM_REQUEST = -1000;


struct AppModuleContainer_t
{
	HMODULE ahAppModule[MODULE_MAX];
	const ModuleDesc_t* apAppModuleDesc[MODULE_MAX];
	int32 AppModuleNum;
	HMODULE CurrentModuleHandle;
	const ModuleDesc_t* CurrentModuleDesc;
	int32 CurrentModuleNo;
	bool bRunFlag;
};


static AppModuleContainer_t AppModuleContainer;


static bool AppModIsExluded(const std::tstring& Path)
{
	//
	//  Exclude all files expect DLL
	//

	std::size_t Pos = Path.find_last_of('.');
	if (Pos == std::tstring::npos)
		return true;

	std::tstring FileFormat = Path.substr(++Pos);
	if (FileFormat.empty())
		return true;

	if (FileFormat == TEXT("dll"))
		return false;

	return true;
};


static std::vector<std::tstring> AppModSearch(const std::tstring& Path)
{
	std::vector<std::tstring> Result;
	Result.reserve(MODULE_MAX);
	
	WIN32_FIND_DATA FindData = {};

	std::tstring FindPath(Path.begin(), Path.end());
	FindPath += TEXT("\\*");

	HANDLE hFind = FindFirstFile(FindPath.c_str(), &FindData);
	if (hFind)
	{
		do
		{
			if (FindData.cFileName[0] == '.')
				continue;

			std::tstring FilePath = Path + TEXT("\\") + FindData.cFileName;
			if (AppModIsExluded(FilePath))
				continue;

			if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (APP_MODULE_RECURSIVE_SRCH)
				{
					std::vector<std::tstring> SubResult = AppModSearch(FilePath);
					if (!SubResult.empty())
						Result.insert(Result.end(), SubResult.begin(), SubResult.end());
				};
			}
			else
			{
				Result.push_back(FilePath);
			};
		} while (FindNextFile(hFind, &FindData));

		FindClose(hFind);
		hFind = NULL;
	};

	return Result;
};


static bool AppModCheckDLL(const std::tstring& Path)
{
    bool bResult = false;
    
    HMODULE hModule = LoadLibraryEx(&Path[0], NULL, DONT_RESOLVE_DLL_REFERENCES);
    ASSERT(hModule != NULL, "%u", GetLastError());
    if (hModule != NULL)
    {
        RequestModuleDesc_t RequestModuleDescProc = RequestModuleDesc_t(GetProcAddress(hModule, MODULE_ENTRY_NAME));
        if (RequestModuleDescProc)
            bResult = true;

        FreeLibrary(hModule);
    };

    return bResult;
};


static std::vector<std::tstring> AppModSearchAndSave(const std::tstring& Path)
{
	std::vector<std::tstring> ModulePathArray = AppModSearch(Path);
	if (ModulePathArray.empty())
		return{};

	for (int32 i = 0; i < int32(ModulePathArray.size()); ++i)
    {    
        if(!AppModCheckDLL(ModulePathArray[i]))
            ModulePathArray.erase(ModulePathArray.begin() + i);        
	};

	return ModulePathArray;
};


void AppModInitialize(void)
{
	AppModuleContainer.CurrentModuleNo = -1;
	AppModuleContainer.CurrentModuleHandle = NULL;

	//
	//  Init phase 1 - load and check all available module dll's
	//
	std::vector<std::tstring> ModulePathArray = AppModSearchAndSave(CfgGetCurrentDir());

	//
	//  Init phase 2 - load and run all available module dll's
	//
	for (int32 i = 0; i < int32(ModulePathArray.size()); ++i)
	{
		std::tstring& Path = ModulePathArray[i];

		HMODULE hModule = LoadLibrary(&Path[0]);
		ASSERT(hModule != NULL, "%u", GetLastError());
		if (hModule == NULL)
			continue;

		RequestModuleDesc_t RequestModuleDescProc = RequestModuleDesc_t(GetProcAddress(hModule, MODULE_ENTRY_NAME));
		ASSERT(RequestModuleDescProc);
		if (!RequestModuleDescProc)
			continue;

		AppModuleContainer.apAppModuleDesc[i] = RequestModuleDescProc();
		ASSERT(AppModuleContainer.apAppModuleDesc[i]);
		if (!AppModuleContainer.apAppModuleDesc[i])
			continue;

		++AppModuleContainer.AppModuleNum;
		AppModuleContainer.ahAppModule[i] = hModule;

#ifdef _DEBUG
		DbgRegistModule(
			AppModuleContainer.apAppModuleDesc[i]->Label,
			AppModuleContainer.ahAppModule[i]
		);
#endif        
	};

	for (int32 i = 0; i < AppModuleContainer.AppModuleNum; ++i)
		AppModuleContainer.apAppModuleDesc[i]->FnInitialize(AppModuleContainer.ahAppModule[i]);
};


void AppModTerminate(void)
{
	AppModUnlock();
	AppModSetCurrent(APP_MODULE_TERM_REQUEST);

	for (int32 i = 0; i < AppModuleContainer.AppModuleNum; ++i)
		AppModuleContainer.apAppModuleDesc[i]->FnTerminate();

	for (int32 i = 0; i < AppModuleContainer.AppModuleNum; ++i)
	{
#ifdef _DEBUG
		DbgRemoveModule(
			AppModuleContainer.ahAppModule[i]
		);
#endif     

		FreeLibrary(AppModuleContainer.ahAppModule[i]);
		AppModuleContainer.ahAppModule[i] = NULL;
	};

	AppModuleContainer.AppModuleNum = 0;
};


void AppModSetCurrent(int32 No)
{
	ASSERT(!AppModuleContainer.bRunFlag);
	ASSERT((No >= 0 && No < AppModuleContainer.AppModuleNum) || (No == APP_MODULE_TERM_REQUEST));

	if (No == AppModuleContainer.CurrentModuleNo)
		return;

	if (AppModuleContainer.CurrentModuleDesc)
	{
		AppModuleContainer.CurrentModuleDesc->FnDetach();

		AppModuleContainer.CurrentModuleNo = -1;
		AppModuleContainer.CurrentModuleDesc = nullptr;
		AppModuleContainer.CurrentModuleHandle = NULL;
	};

	if (No == APP_MODULE_TERM_REQUEST)
		return;

	AppModuleContainer.CurrentModuleNo = No;

	AppModuleContainer.CurrentModuleDesc = AppModuleContainer.apAppModuleDesc[No];
	ASSERT(AppModuleContainer.CurrentModuleDesc);

	AppModuleContainer.CurrentModuleHandle = AppModuleContainer.ahAppModule[No];
	ASSERT(AppModuleContainer.CurrentModuleHandle != NULL);

	AppModuleContainer.CurrentModuleDesc->FnAttach();
};


void AppModLock(void)
{
	if (!AppModuleContainer.CurrentModuleDesc || AppModuleContainer.bRunFlag)
		return;

	AppModuleContainer.bRunFlag = true;
	AppModuleContainer.CurrentModuleDesc->FnLock();
};


void AppModUnlock(void)
{
	if (!AppModuleContainer.CurrentModuleDesc || !AppModuleContainer.bRunFlag)
		return;

	AppModuleContainer.CurrentModuleDesc->FnUnlock();
	AppModuleContainer.bRunFlag = false;
};


HWND AppModGetDialog(int32 No, HWND hWndParent, int32 Type)
{
	ASSERT(No >= 0 && No < AppModuleContainer.AppModuleNum);
	ASSERT(AppModuleContainer.apAppModuleDesc[No] != nullptr);
	return AppModuleContainer.apAppModuleDesc[No]->FnRequestDlg(hWndParent, Type);
};


HINSTANCE AppModGetInstance(int32 No)
{
	ASSERT(No >= 0 && No < AppModuleContainer.AppModuleNum);
	ASSERT(AppModuleContainer.ahAppModule[No] != NULL);
	return AppModuleContainer.ahAppModule[No];
};


int32 AppModGetCount(void)
{
	return AppModuleContainer.AppModuleNum;
};


int32 AppModGetNo(const char* Label)
{
	ASSERT(Label);
	
	for (int32 i = 0; i < AppModuleContainer.AppModuleNum; ++i)
	{
		if (!std::strcmp(Label, AppModuleContainer.apAppModuleDesc[i]->Label))
			return i;
	};

	return -1;
};


const char* AppModGetLabel(int32 No)
{
	ASSERT(No >= 0 && No < AppModuleContainer.AppModuleNum);
	ASSERT(AppModuleContainer.ahAppModule[No] != NULL);
	if (No >= 0 && No < AppModuleContainer.AppModuleNum)
		return AppModuleContainer.apAppModuleDesc[No]->Label;
	else
		return nullptr;
};