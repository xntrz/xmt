#include "Thread.hpp"
#include "Event.hpp"
#include "Time.hpp"


static const int32 THREAD_CALLBACK_MAX = BITSOF(uint32);
static const int32 THREAD_NAME_MAX = 64;
static const uint32 THREAD_START_TIMEOUT = 5000;	// MS
static const uint32 THREAD_EXIT_TIMEOUT = 5000;		// MS
static const char* MAIN_THREAD_NAME = "Main thread";


thread_local static char CURRENT_THREAD_NAME[THREAD_NAME_MAX] = "Unknown thread name";
thread_local static uint32 CURRENT_THREAD_ID = 0;
thread_local static HOBJ CURRENT_THREAD = 0;


struct Thread_t : public CListNode<Thread_t>
{
	HANDLE Handle;
	HOBJ hEventStart;
	uint32 Id;
	uint32 Flags;
	void* Param;
	ThreadMainProc_t ThreadMainProc;
	char szName[THREAD_NAME_MAX];
	uint32 CallbackHistory;
	
#ifdef _DEBUG	
	uint32 DbgSameResultAccum;
	uint32 DbgPrevEIP;
	uint32 DbgTime;
#endif
};


struct ThreadContainer_t
{
	CList<Thread_t> ListThread;
	uint32 NumThread;
	HOBJ SvcThread;
	HOBJ SvcEvent;
	bool SvcRunFlag;
	std::recursive_mutex Mutex;
	uint32 ThreadSuspendInitiator;
	int32 NumProcessor;
	int32 NumCore;
	struct
	{
		void(*FnThreadStart)(HOBJ hThread);
		void(*FnThreadStop)(HOBJ hThread);
		std::atomic<uint32> FlagAlloc;
	} CallbackArray[THREAD_CALLBACK_MAX];
	uint32 CallbackHistory;
};


static ThreadContainer_t s_ThreadContainer;
static ThreadContainer_t& ThreadContainer = s_ThreadContainer;


static void ThreadSetDbgName(const char* pszName, uint32 ThreadId = 0)
{
	if (!IsDebuggerPresent())
		return;

#define MS_VC_EXCEPTION 0x406D1388

#pragma pack(push,8)
	typedef struct {
		DWORD    dwType;     // Must be 0x1000.
		LPCSTR   szName;     // Pointer to name (in user addr space).
		DWORD    dwThreadID; // Thread ID (-1=caller thread).
		DWORD    dwFlags;    // Reserved for future use, must be zero.
	} THREADNAME_INFO;
#pragma pack(pop)

	THREADNAME_INFO ThreadNameInfo = {};
	ThreadNameInfo.dwType = 0x1000;
	ThreadNameInfo.szName = pszName;
	ThreadNameInfo.dwThreadID = (ThreadId ? ThreadId : GetCurrentThreadId());
	ThreadNameInfo.dwFlags = 0;
	
	__try
	{
		RaiseException(
			MS_VC_EXCEPTION,
			0,
			sizeof(ThreadNameInfo) / sizeof(ULONG_PTR),
			PULONG_PTR(&ThreadNameInfo)
		);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		;
	};
};


static void ThreadSvcExtra(Thread_t* Thread)
{
	if (IS_FLAG_SET(Thread->Flags, ThreadFlag_AutoDestroy))
	{
		uint32 WaitMs = 0;
		
		if(ThreadIsExited(Thread))
			WaitMs = 100;

		if (ThreadWait(Thread, WaitMs))
			ThreadDestroy(Thread);
	};
};


static void ThreadSvcProc(void* Param)
{
	while (ThreadContainer.SvcRunFlag)
	{
		EventWaitFor(ThreadContainer.SvcEvent, 1000);
		if (!ThreadContainer.SvcRunFlag)
			break;

		{
			std::lock_guard<std::recursive_mutex> Lock(ThreadContainer.Mutex);

			auto it = ThreadContainer.ListThread.begin();
			while (it)
			{
				Thread_t* Thread = &*it;
				++it;
		
				ThreadSvcExtra(Thread);
			};
		}
	};
};


static void ThreadNotifyCallback(Thread_t* Thread, bool FlagStart)
{
	//
	//	Iterate over all callbacks and call only that was registered before thread created
	//
	
	std::unique_lock<std::recursive_mutex> Lock(ThreadContainer.Mutex);

	for (int32 i = 0; i < COUNT_OF(ThreadContainer.CallbackArray); ++i)
	{
		if ((ThreadContainer.CallbackArray[i].FlagAlloc) && BIT_TEST(Thread->CallbackHistory, i))
		{
			if (FlagStart)
			{
				if (ThreadContainer.CallbackArray[i].FnThreadStart)
					ThreadContainer.CallbackArray[i].FnThreadStart(Thread);
			}
			else
			{
				if (ThreadContainer.CallbackArray[i].FnThreadStop)
					ThreadContainer.CallbackArray[i].FnThreadStop(Thread);
			};
		};
	};
};


static DWORD WINAPI ThreadRouteProc(LPVOID lpParam)
{
	Thread_t* Thread = (Thread_t*)lpParam;

	CURRENT_THREAD = Thread;
	CURRENT_THREAD_ID = Thread->Id;
	std::strcpy(CURRENT_THREAD_NAME, Thread->szName);
	ThreadSetDbgName(Thread->szName, Thread->Id);

	{
		EventSignalAll(Thread->hEventStart);

		ThreadNotifyCallback(Thread, true);
		Thread->ThreadMainProc(Thread->Param);
		ThreadNotifyCallback(Thread, false);

		EventSignalAll(ThreadContainer.SvcEvent);
	}

	CURRENT_THREAD_ID = -1;
	std::strcpy(CURRENT_THREAD_NAME, "Exited thread");
	CURRENT_THREAD = 0;

	return 0;
};


void ThreadInitialize(void)
{
	CURRENT_THREAD_ID = GetCurrentThreadId();
	std::strcpy(CURRENT_THREAD_NAME, MAIN_THREAD_NAME);
	ThreadSetDbgName(MAIN_THREAD_NAME, CURRENT_THREAD_ID);

	ThreadContainer.ListThread.clear();
	ThreadContainer.NumThread = 0;
	ThreadContainer.SvcThread = 0;
	ThreadContainer.SvcEvent = 0;
	ThreadContainer.ThreadSuspendInitiator = 0;

	ThreadContainer.SvcRunFlag = true;
	ThreadContainer.SvcEvent = EventCreate();
	ThreadContainer.SvcThread = ThreadCreate("ThreadSvc", ThreadSvcProc, 0);

	ASSERT(ThreadContainer.SvcEvent);
	ASSERT(ThreadContainer.SvcThread);

	SYSTEM_INFO SystemInfo = {};
	GetSystemInfo(&SystemInfo);
	ThreadContainer.NumProcessor = int32(SystemInfo.dwNumberOfProcessors);
	ThreadContainer.NumCore = (ThreadContainer.NumProcessor / 2);

	OUTPUTLN("cpu core num: %d, cpu logical num: %d", ThreadContainer.NumCore, ThreadContainer.NumProcessor);
};


void ThreadTerminate(void)
{
	ASSERT(ThreadContainer.SvcEvent);
	ASSERT(ThreadContainer.SvcThread);

	ThreadContainer.SvcRunFlag = false;
	EventSignalAll(ThreadContainer.SvcEvent);
	ThreadWait(ThreadContainer.SvcThread);
	
	EventDestroy(ThreadContainer.SvcEvent);
	ThreadDestroy(ThreadContainer.SvcThread);

	ASSERT(ThreadContainer.ListThread.empty());
	ASSERT(ThreadContainer.NumThread == 0);	
};


HOBJ ThreadCallbackRegist(void(*FnThreadStart)(HOBJ hThread), void(*FnThreadStop)(HOBJ hThread))
{	
	HOBJ hResult = 0;

	{
		std::unique_lock<std::recursive_mutex> Lock(ThreadContainer.Mutex);

		for (int32 i = 0; i < COUNT_OF(ThreadContainer.CallbackArray); ++i)
		{
			if (!ThreadContainer.CallbackArray[i].FlagAlloc)
			{
				ThreadContainer.CallbackArray[i].FlagAlloc = 1;
				ThreadContainer.CallbackArray[i].FnThreadStart = FnThreadStart;
				ThreadContainer.CallbackArray[i].FnThreadStop = FnThreadStop;

				BIT_SET(ThreadContainer.CallbackHistory, i);

				//
				//	0 is reserved for indication error, so regist returning value is = i + 1
				// 	For removing handle will same reversed operation = i - 1
				//
				hResult = HOBJ(i + 1);
				break;
			};
		};
	}

	return hResult;
};


void ThreadCallbackRemove(HOBJ hCallback)
{
	std::unique_lock<std::recursive_mutex> Lock(ThreadContainer.Mutex);

	int32 Index = int32(hCallback) - 1;
	
	ASSERT(Index >= 0 && Index < COUNT_OF(ThreadContainer.CallbackArray));
	ASSERT(ThreadContainer.CallbackArray[Index].FlagAlloc);
	
	if ((Index >= 0 && Index < COUNT_OF(ThreadContainer.CallbackArray)) &&
		(ThreadContainer.CallbackArray[Index].FlagAlloc))
	{
		ThreadContainer.CallbackArray[Index].FlagAlloc = 0;
		ThreadContainer.CallbackArray[Index].FnThreadStart = nullptr;
		ThreadContainer.CallbackArray[Index].FnThreadStop = nullptr;

		BIT_CLEAR(ThreadContainer.CallbackHistory, Index);
	};
};


HOBJ ThreadCreate(const char* pszName, ThreadMainProc_t ThreadMainProc, uint32 Flags, void* Param)
{
	ASSERT(ThreadMainProc);
	ASSERT(std::strlen(pszName) < THREAD_NAME_MAX);

	std::lock_guard<std::recursive_mutex> Lock(ThreadContainer.Mutex);

	Thread_t* Thread = new Thread_t();
	if (!Thread)
		return 0;

	ThreadContainer.ListThread.push_back(Thread);
	++ThreadContainer.NumThread;
	
	Thread->Handle = NULL;
	Thread->hEventStart = EventCreate();
	ASSERT(Thread->hEventStart);
	Thread->Id = 0;
	Thread->Flags = Flags;
	Thread->Param = Param;
	Thread->ThreadMainProc = ThreadMainProc;
	std::strcpy(Thread->szName, pszName);
	Thread->CallbackHistory = ThreadContainer.CallbackHistory;
	
#ifdef _DEBUG
	Thread->DbgPrevEIP = 0;
	Thread->DbgSameResultAccum = 0;
	Thread->DbgTime = TimeCurrentTick();
#endif

	Thread->Handle = CreateThread(NULL, NULL, LPTHREAD_START_ROUTINE(ThreadRouteProc), LPVOID(Thread), NULL, LPDWORD(&Thread->Id));
	if (Thread->Handle)
	{
		if (!EventWaitFor(Thread->hEventStart, THREAD_START_TIMEOUT))
		{
			ASSERT(false, "Failed to wait for thread start");

			EventDestroy(Thread->hEventStart);
			delete Thread;
			Thread = nullptr;
		};
	}
	else
	{
		ASSERT(false, "Failed to create OS thread (Error: %u)", GetLastError());
		EventDestroy(Thread->hEventStart);
		delete Thread;
		Thread = nullptr;
	};	

	return Thread;
};


void ThreadDestroy(HOBJ hThread)
{
	Thread_t* Thread = (Thread_t*)hThread;
	ASSERT(Thread);
	
	std::lock_guard<std::recursive_mutex> Lock(ThreadContainer.Mutex);

	EventDestroy(Thread->hEventStart);

	--ThreadContainer.NumThread;
	ThreadContainer.ListThread.erase(Thread);

	delete Thread;
};


void ThreadSuspend(HOBJ hThread)
{
	Thread_t* Thread = (Thread_t*)hThread;
	ASSERT(Thread);

	SuspendThread(Thread->Handle);
};


void ThreadResume(HOBJ hThread)
{
	Thread_t* Thread = (Thread_t*)hThread;
	ASSERT(Thread);

	ResumeThread(Thread->Handle);
};


void ThreadWait(HOBJ hThread)
{
	Thread_t* Thread = (Thread_t*)hThread;
	ASSERT(Thread);
	
	WaitForSingleObject(Thread->Handle, INFINITE);
};


bool ThreadWait(HOBJ hThread, uint32 Ms)
{
	Thread_t* Thread = (Thread_t*)hThread;
	ASSERT(Thread);

	return (WaitForSingleObject(Thread->Handle, Ms) == WAIT_OBJECT_0);
};


bool ThreadIsExited(HOBJ hThread)
{
	return ThreadWait(hThread, 0);
};


uint32 ThreadGetId(HOBJ hThread)
{
	Thread_t* Thread = (Thread_t*)hThread;
	ASSERT(Thread);

	return Thread->Id;
};


const char* ThreadGetName(HOBJ hThread)
{
	Thread_t* Thread = (Thread_t*)hThread;
	ASSERT(Thread);

	return Thread->szName;
};


uint32 ThreadGetId(void)
{
	ASSERT(CURRENT_THREAD_ID != 0);
	return CURRENT_THREAD_ID;
};


const char* ThreadGetName(void)
{
	return CURRENT_THREAD_NAME;
};


HOBJ ThreadCurrent(void)
{
	ASSERT(CURRENT_THREAD != 0);
	return CURRENT_THREAD;
};


void ThreadSleep(uint32 Ms)
{
	Sleep(Ms);
};


void ThreadEnumerate(ThreadEnumProc_t ThreadEnumProc)
{
	std::unique_lock<std::recursive_mutex> Lock(ThreadContainer.Mutex);

	for (auto& it : ThreadContainer.ListThread)
	{
		Thread_t* Self = &it;

		if (!ThreadEnumProc(Self))
			break;
	};
};


void ThreadSuspendAllExceptThis(void)
{
	std::unique_lock<std::recursive_mutex> Lock(ThreadContainer.Mutex);

	uint32 ThreadIdCurrent = ThreadGetId();
	ThreadContainer.ThreadSuspendInitiator = ThreadIdCurrent;

	for (auto& it : ThreadContainer.ListThread)
	{
		Thread_t* Self = &it;

		if (ThreadGetId(Self) != ThreadIdCurrent)
			ThreadSuspend(Self);
	};
};


void ThreadResumeAllExceptThis(void)
{
	std::unique_lock<std::recursive_mutex> Lock(ThreadContainer.Mutex);

	uint32 ThreadIdCurrent = ThreadGetId();
	ASSERT(ThreadContainer.ThreadSuspendInitiator == ThreadIdCurrent);
	ThreadContainer.ThreadSuspendInitiator = 0;

	for (auto& it : ThreadContainer.ListThread)
	{
		Thread_t* Self = &it;

		if (ThreadGetId(Self) != ThreadIdCurrent)
			ThreadResume(Self);
	};
};


int32 ThreadGetCoreNum(void)
{
	return ThreadContainer.NumCore;
};


int32 ThreadGetProcessorNum(void)
{
	return ThreadContainer.NumProcessor;
};