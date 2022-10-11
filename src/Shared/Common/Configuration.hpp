#pragma once


DLLSHARED bool CfgCheckWinver(void);
DLLSHARED void CfgInitialize(const char* AppName, HINSTANCE hInstance);
DLLSHARED void CfgTerminate(void);
DLLSHARED bool CfgIsArgPresent(const char* pszArg);
DLLSHARED const char* CfgGetCurrentDirA(void);
DLLSHARED const wchar_t* CfgGetCurrentDirW(void);
DLLSHARED const char* CfgGetAppNameA(void);
DLLSHARED const wchar_t* CfgGetAppNameW(void);
DLLSHARED HINSTANCE CfgGetAppInstance(void);
DLLSHARED void CfgLoad(HMODULE hModule, const char* Label, int32 DefaultRcId = -1);
DLLSHARED void CfgSave(void);
DLLSHARED void CfgReset(void);
DLLSHARED bool CfgIsTestMode(void);


#ifdef _UNICODE
#define CfgGetCurrentDir CfgGetCurrentDirW
#else
#define CfgGetCurrentDir CfgGetCurrentDirA
#endif