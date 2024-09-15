#pragma once

void CfgInitialize(const std::string& appname, HINSTANCE hInstance);
void CfgTerminate(void);
DLLSHARED bool CfgIsArgPresent(const char* pszArg);
DLLSHARED const char* CfgGetCurrentDir(void);
DLLSHARED HINSTANCE CfgGetAppInstance(void);
DLLSHARED void CfgLoad(HMODULE hMod, const std::string& modname, int32 rcid = -1);
DLLSHARED void CfgSave(void);
DLLSHARED bool MakeWindowScreenshot(HWND hWnd);