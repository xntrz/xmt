#include "Registry.hpp"

#include <map>


#define REG_NAME_MAX (256)
#define REG_CMD_ARGS_NUM (32)
#define REG_CMD_LINE_MAX (256)
#define REG_REC_NUM (64)
#define REG_RESET_INFO_NUM (64)


struct RegVar_t
{
	uint32 Flags;
	RegVarNotifyCallback_t NotifyCallback;
	struct
	{
		int32 Integer;
		float Real;
		char String[REG_NAME_MAX];
	} Value;
};


enum RegRecType_t
{
	RegRecType_None = 0,
	RegRecType_Cmd,
	RegRecType_Var,
};


struct RegRec_t
{
	RegRec_t* Next;
	RegRecType_t Type;
	char Name[REG_NAME_MAX];
	int32 RefCount;
	int32 Generation;
	struct
	{
		RegVar_t Variable;
		RegCmdHandler_t CommandHandler;
	} Ctx;
};


struct RegResetInfo_t
{
	RegResetCallback_t Callback;
};


struct RegContainer_t
{
	int32 Generation;
	RegRec_t RecordArray[REG_REC_NUM];
	RegRec_t* ListRecordFree;
	RegRec_t* ListRecordAlloc;
	int32 ListRecordAllocSize;
	std::recursive_mutex Mutex;
	std::atomic<bool> FlagCmdExec;
	RegResetInfo_t ResetInfoArray[REG_RESET_INFO_NUM];
	int32 ResetInfoNum;
};


static RegContainer_t RegContainer;


static inline bool RegIsNameValid(const char* Name)
{
	return ( (std::strlen(Name) + 1) <= REG_NAME_MAX );
};


static void RegVarClear(RegVar_t* RegVar)
{
	RegVar->Flags = 0;
	RegVar->NotifyCallback = nullptr;
	RegVar->Value.Integer = -1;
	RegVar->Value.Real = FLT_MAX;
	RegVar->Value.String[0] = '\0';
};


static void RegVarSet(RegVar_t* RegVar, const char* Value)
{
	RegVar->Value.Integer = std::atoi(Value);
	RegVar->Value.Real = float(std::atof(Value));
	
	ASSERT(std::strlen(Value) < REG_NAME_MAX);
	if (std::strlen(Value) < REG_NAME_MAX)
		std::strcpy(RegVar->Value.String, Value);

	if (RegVar->NotifyCallback)
		RegVar->NotifyCallback(RegVar, Value);
};


static void RegRecClear(RegRec_t* RegRec)
{
	RegRec->Type = RegRecType_None;
	RegRec->Name[0] = '\0';
	RegRec->RefCount = 0;
	RegRec->Generation = 0;	
};


static RegRec_t* RegRecAlloc(void)
{
	//
	//	Alloc new reg record
	// 	Pop front from free list and push front to alloc list
	//

	RegRec_t* RegRec = RegContainer.ListRecordFree;
	if (RegRec)
	{
		RegContainer.ListRecordFree = RegRec->Next;
		
		RegRec->Next = RegContainer.ListRecordAlloc;
		RegContainer.ListRecordAlloc = RegRec;
		++RegContainer.ListRecordAllocSize;
	};

	return RegRec;
};


static void RegRecFree(RegRec_t* RegRec)
{
	//
	//	Free reg record
	// 	Remove from alloc list and push front to free list
	//

	RegRec_t* It = RegContainer.ListRecordAlloc;
	RegRec_t* ItPrev = nullptr;
	while (It)
	{
		if (It == RegRec)
		{
			ASSERT(RegContainer.ListRecordAllocSize > 0);
			--RegContainer.ListRecordAllocSize;
			
			if (ItPrev)
				ItPrev->Next = It->Next;
			else
				RegContainer.ListRecordAlloc = It->Next;

			break;
		};

		ItPrev = It;
		It = It->Next;
	};

	RegRec->Next = RegContainer.ListRecordFree;
	RegContainer.ListRecordFree = RegRec;
};


static RegRec_t* RegRecCreate(const char* Name, RegRecType_t Type)
{
	RegRec_t* RegRec = RegRecAlloc();
	if (RegRec)
	{
		RegRec->Type = Type;
		RegRec->Generation = RegContainer.Generation;
		ASSERT(std::strlen(Name) < REG_NAME_MAX);
		std::memmove(RegRec->Name, Name, std::strlen(Name) + 1);
	};

	return RegRec;
};


static void RegRecDestroy(RegRec_t* RegRec)
{
	RegRecFree(RegRec);
};


static RegRec_t* RegRecSearch(const char* Name, RegRecType_t Type)
{
	RegRec_t* It = RegContainer.ListRecordAlloc;
	while (It)
	{
		if ((!std::strcmp(It->Name, Name)) && (It->Type == Type))
			return It;

		It = It->Next;
	};

	return nullptr;
};


static RegRec_t* RegCmdCreate(const char* Name, RegCmdHandler_t Handler)
{
	RegRec_t* RegCmd = RegRecSearch(Name, RegRecType_Cmd);
	if (!RegCmd)
	{
		RegCmd = RegRecCreate(Name, RegRecType_Cmd);
		if (RegCmd)
			RegCmd->Ctx.CommandHandler = Handler;
	};

	return RegCmd;
};


static void RegCmdDestroy(RegRec_t* RegCmd)
{
	RegRecDestroy(RegCmd);
};


static RegRec_t* RegCmdSearch(const char* Name)
{
	return RegRecSearch(Name, RegRecType_Cmd);
};


static void RegCmdParseLine(char* Line, char** Cmd, char** Argv, int32* Argc)
{
	enum RegCmdParseState_t
	{
		RegCmdParseState_Cmd = 0,
		RegCmdParseState_Arg,
	};

	RegCmdParseState_t State = RegCmdParseState_Cmd;
	const char* Token = " ";	
	int32 ArgCount = 0;
	char* NextToken = nullptr;
	char* Ptr = strtok_s(Line, Token, &NextToken);
	while (ArgCount < *Argc)
	{
		switch (State)
		{
		case RegCmdParseState_Cmd:
			{
				*Cmd = Ptr;
				State = RegCmdParseState_Arg;
			}
			break;

		case RegCmdParseState_Arg:
			{
				Argv[ArgCount++] = Ptr;
			}
			break;
		};

		Ptr = strtok_s(nullptr, Token, &NextToken);
		if (!Ptr)
			break;
	};

	*Argc = ArgCount;
};


static RegRec_t* RegVarCreate(const char* Name, RegVarNotifyCallback_t NotifyCallback)
{
	RegRec_t* RegVar = RegRecSearch(Name, RegRecType_Var);
	if (!RegVar)
	{
		RegVar = RegRecCreate(Name, RegRecType_Var);
		if (RegVar)
		{
			RegVar->RefCount = 1;
			RegVar->Ctx.Variable.NotifyCallback = NotifyCallback;
			RegVarClear(&RegVar->Ctx.Variable);			
		};
	};

	return RegVar;
};


static void RegVarDestroy(RegRec_t* RegVar)
{
	RegRecDestroy(RegVar);
};


static RegRec_t* RegVarSearch(const char* Name)
{
	return RegRecSearch(Name, RegRecType_Var);
};


static void RegVarRefInc(RegRec_t* RegVar)
{
	++RegVar->RefCount;
};


static void RegVarRefDec(RegRec_t* RegVar)
{
	if (!--RegVar->RefCount)
		RegVarDestroy(RegVar);
};


void RegInitialize(void)
{
	RegContainer.Generation = 0;
	RegContainer.FlagCmdExec = false;

	for (int32 i = 0; i < (sizeof(RegContainer.RecordArray) / sizeof(RegContainer.RecordArray[0])); ++i)
	{
		RegRec_t* RegRec = &RegContainer.RecordArray[i];
		RegRec->Next = nullptr;
		RegRecClear(RegRec);
		RegRecFree(RegRec);
	};
};


void RegTerminate(void)
{
	ASSERT(
		(RegContainer.ListRecordAllocSize == 0),
		"Unfreed var num: %d",
		RegContainer.ListRecordAllocSize
	);
};


void RegResetRegist(RegResetCallback_t ResetCallback)
{
	if (RegContainer.FlagCmdExec)
		return;

	std::unique_lock<std::recursive_mutex> Lock(RegContainer.Mutex);

	if (RegContainer.ResetInfoNum >= COUNT_OF(RegContainer.ResetInfoArray))
		return;

	int32 Index = RegContainer.ResetInfoNum++;
	RegContainer.ResetInfoArray[Index].Callback = ResetCallback;
};


void RegResetExec(void)
{
	if (RegContainer.FlagCmdExec)
		return;

	std::unique_lock<std::recursive_mutex> Lock(RegContainer.Mutex);

	for (int32 i = 0; i < RegContainer.ResetInfoNum; ++i)
		RegContainer.ResetInfoArray[i].Callback();
};


void RegRefInc(void)
{
	if (RegContainer.FlagCmdExec)
		return;
	
	std::unique_lock<std::recursive_mutex> Lock(RegContainer.Mutex);
	
	++RegContainer.Generation;
};


void RegRefDec(void)
{
	if (RegContainer.FlagCmdExec)
		return;
	
	std::unique_lock<std::recursive_mutex> Lock(RegContainer.Mutex);
	
	if (--RegContainer.Generation >= 0)
	{
		RegRec_t* It = RegContainer.ListRecordAlloc;
		while (It)
		{
			//
			//	This node may be free during current iteration thus cache it
			//
			RegRec_t* RegRec = It;
			It = It->Next;

			if (RegRec->Generation > RegContainer.Generation)
			{
				switch (RegRec->Type)
				{
				case RegRecType_Cmd:
					{
						RegCmdDestroy(RegRec);
					}
					break;

				case RegRecType_Var:
					{
						RegVarRefDec(RegRec);
					}
					break;

				default:
					ASSERT(false);
					break;
				};
			};
		};
	};
};


bool RegCmdRegist(const char* Cmd, RegCmdHandler_t Handler)
{
	if (!RegIsNameValid(Cmd))
		return false;

	if (RegContainer.FlagCmdExec)
		return false;

	std::unique_lock<std::recursive_mutex> Lock(RegContainer.Mutex);
	
	return (RegCmdCreate(Cmd, Handler) != nullptr);
};


void RegCmdRemove(const char* Cmd)
{
	if (RegContainer.FlagCmdExec)
		return;
	
	std::unique_lock<std::recursive_mutex> Lock(RegContainer.Mutex);

	RegRec_t* RegCmd = RegCmdSearch(Cmd);
	if (RegCmd)
		RegCmdDestroy(RegCmd);
};


bool RegCmdExec(const char* Line)
{
	bool bResult = false;
	
	if (RegContainer.FlagCmdExec)
		return bResult;

	std::unique_lock<std::recursive_mutex> Lock(RegContainer.Mutex);
	
	RegContainer.FlagCmdExec = true;
	{
		char CmdLine[REG_CMD_LINE_MAX] = { 0 };
		char* Args[REG_CMD_ARGS_NUM] = { 0 };
		char* Cmd = nullptr;
		int32 Argc = sizeof(Args) / sizeof(Args[0]);

		std::strcpy(CmdLine, Line);

		RegCmdParseLine(CmdLine, &Cmd, Args, &Argc);
		if (Argc)
		{
			RegRec_t* RegRec = RegCmdSearch(Cmd);
			if (RegRec)
			{
				RegRec->Ctx.CommandHandler(Cmd, (const char**)Args, Argc);
				bResult = true;
			};
		};
	}
	RegContainer.FlagCmdExec = false;

	return bResult;
};


HOBJ RegVarRegist(const char* Var, RegVarNotifyCallback_t Callback)
{
	if (!RegIsNameValid(Var))
		return false;

	std::unique_lock<std::recursive_mutex> Lock(RegContainer.Mutex);

	return RegVarCreate(Var, Callback);
};


void RegVarRemove(const char* Var)
{
	std::unique_lock<std::recursive_mutex> Lock(RegContainer.Mutex);

	RegRec_t* RegVar = RegVarSearch(Var);
	if (RegVar)
		RegVarRefDec(RegVar);
};


HOBJ RegVarFind(const char* Var)
{
	std::unique_lock<std::recursive_mutex> Lock(RegContainer.Mutex);
	
	return RegVarSearch(Var);
};


void RegVarEnum(RegVarEnumCallback_t Callback, void* Param)
{
	std::unique_lock<std::recursive_mutex> Lock(RegContainer.Mutex);

	RegRec_t* RegRec = RegContainer.ListRecordAlloc;
	while (RegRec)
	{
		if (RegRec->Type == RegRecType_Var)
			Callback(RegRec, RegRec->Name, RegRec->Ctx.Variable.Value.String, Param);

		RegRec = RegRec->Next;
	};
};


void RegVarLock(const char* Var)
{
	std::unique_lock<std::recursive_mutex> Lock(RegContainer.Mutex);

	RegRec_t* RegVar = RegVarSearch(Var);
	if (RegVar)
		RegVarRefInc(RegVar);
};


void RegVarUnlock(const char* Var)
{
	std::unique_lock<std::recursive_mutex> Lock(RegContainer.Mutex);

	RegRec_t* RegVar = RegVarSearch(Var);
	if (RegVar)
		RegVarRefDec(RegVar);
};


void RegVarSetFlags(HOBJ hVar, uint32 Flags)
{
	RegRec_t* RegVar = (RegRec_t*)hVar;

	RegVar->Ctx.Variable.Flags = Flags;
};


uint32 RegVarGetFlags(HOBJ hVar)
{
	RegRec_t* RegVar = (RegRec_t*)hVar;

	return RegVar->Ctx.Variable.Value.Integer;
};


int32 RegVarReadInt32(HOBJ hVar)
{
	RegRec_t* RegVar = (RegRec_t*)hVar;
	
	return RegVar->Ctx.Variable.Value.Integer;
};


uint32 RegVarReadUInt32(HOBJ hVar)
{
	RegRec_t* RegVar = (RegRec_t*)hVar;
	
	return uint32(RegVar->Ctx.Variable.Value.Integer);
};


float RegVarReadFloat(HOBJ hVar)
{
	RegRec_t* RegVar = (RegRec_t*)hVar;

	return RegVar->Ctx.Variable.Value.Real;
};


const char* RegVarReadString(HOBJ hVar)
{
	RegRec_t* RegVar = (RegRec_t*)hVar;

	return RegVar->Ctx.Variable.Value.String;
};


void RegVarReadString(HOBJ hVar, char* Buffer, int32 BufferSize)
{
	RegRec_t* RegVar = (RegRec_t*)hVar;

	std::strcpy(Buffer, RegVar->Ctx.Variable.Value.String);
};


void RegVarSetValue(HOBJ hVar, const char* pszValue)
{
	RegRec_t* RegVar = (RegRec_t*)hVar;

	RegVarSet(&RegVar->Ctx.Variable, pszValue);
};


double RegArgToDouble(const char* pszArgValue)
{
	return std::atof(pszArgValue);
};


float RegArgToReal(const char* pszArgValue)
{
	return float(RegArgToDouble(pszArgValue));
};


unsigned RegArgToUInt32(const char* pszArgValue)
{
	return std::strtoul(pszArgValue, nullptr, 0);
};


int RegArgToInt32(const char* pszArgValue)
{
	return std::atoi(pszArgValue);
};


bool RegArgToBool(const char* pszArgValue)
{
	if (!std::strcmp(pszArgValue, "true"))
		return true;

	return false;
};


bool RegIsArgBool(const char* pszArgValue)
{
	return (!std::strcmp(pszArgValue, "true") ||
			!std::strcmp(pszArgValue, "false"));
};


void RegBoolToArg(bool bValue, char* Buffer, int32 BufferSize)
{
	static const char BOOL_TRUE[] = "true";
	static const char BOOL_FALSE[] = "false";

	strncpy_s(
		Buffer, 
		BufferSize,
		(bValue ? BOOL_TRUE : BOOL_FALSE),
		(bValue ? (sizeof(BOOL_TRUE) - 1) : (sizeof(BOOL_FALSE) - 1))
	);
};