#pragma once


typedef void(*RegCmdHandler_t)(const char* Cmd, const char** argv, int32 argc);
typedef void(*RegVarNotifyCallback_t)(HOBJ hVar, const char* Value);
typedef void(*RegVarEnumCallback_t)(HOBJ hVar, const char* Name, const char* Value, void* Param);
typedef void(*RegResetCallback_t)(void);


DLLSHARED void RegInitialize(void);
DLLSHARED void RegTerminate(void);
DLLSHARED void RegResetRegist(RegResetCallback_t ResetCallback);
DLLSHARED void RegResetExec(void);
DLLSHARED void RegRefInc(void);
DLLSHARED void RegRefDec(void);
DLLSHARED bool RegCmdRegist(const char* Cmd, RegCmdHandler_t Handler);
DLLSHARED void RegCmdRemove(const char* Cmd);
DLLSHARED bool RegCmdExec(const char* Line);
DLLSHARED HOBJ RegVarRegist(const char* Var, RegVarNotifyCallback_t Callback = nullptr);
DLLSHARED void RegVarRemove(const char* Var);
DLLSHARED HOBJ RegVarFind(const char* Var);
DLLSHARED void RegVarEnum(RegVarEnumCallback_t Callback, void* Param);
DLLSHARED void RegVarLock(const char* Var);
DLLSHARED void RegVarUnlock(const char* Var);
DLLSHARED void RegVarSetFlags(HOBJ hVar, uint32 Flags);
DLLSHARED uint32 RegVarGetFlags(HOBJ hVar);
DLLSHARED int32 RegVarReadInt32(HOBJ hVar);
DLLSHARED uint32 RegVarReadUInt32(HOBJ hVar);
DLLSHARED float RegVarReadFloat(HOBJ hVar);
DLLSHARED const char* RegVarReadString(HOBJ hVar);
DLLSHARED void RegVarReadString(HOBJ hVar, char* Buffer, int32 BufferSize);
DLLSHARED void RegVarSetValue(HOBJ hVar, const char* pszValue);
DLLSHARED double RegArgToDouble(const char* pszArgValue);
DLLSHARED float RegArgToReal(const char* pszArgValue);
DLLSHARED unsigned RegArgToUInt32(const char* pszArgValue);
DLLSHARED int RegArgToInt32(const char* pszArgValue);
DLLSHARED bool RegArgToBool(const char* pszArgValue);
DLLSHARED bool RegIsArgBool(const char* pszArgValue);
DLLSHARED void RegBoolToArg(bool bValue, char* Buffer, int32 BufferSize);