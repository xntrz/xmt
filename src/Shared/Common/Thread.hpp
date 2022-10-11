#pragma once


typedef void(*ThreadMainProc_t)(void* Param);
typedef bool(*ThreadEnumProc_t)(HOBJ hThread);

enum ThreadFlag_t
{
    ThreadFlag_AutoDestroy = BIT(0),
};

DLLSHARED void ThreadInitialize(void);
DLLSHARED void ThreadTerminate(void);
DLLSHARED HOBJ ThreadCallbackRegist(void(*FnThreadStart)(HOBJ hThread), void(*FnThreadStop)(HOBJ hThread));
DLLSHARED void ThreadCallbackRemove(HOBJ hCallback);
DLLSHARED HOBJ ThreadCreate(const char* Name, ThreadMainProc_t ThreadMainPrc, uint32 Flags = 0, void* Param = nullptr);
DLLSHARED void ThreadDestroy(HOBJ hThread);
DLLSHARED void ThreadSuspend(HOBJ hThread);
DLLSHARED void ThreadResume(HOBJ hThread);
DLLSHARED void ThreadWait(HOBJ hThread);
DLLSHARED bool ThreadWait(HOBJ hThread, uint32 Ms);
DLLSHARED bool ThreadIsExited(HOBJ hThread);
DLLSHARED uint32 ThreadGetId(HOBJ hThread);
DLLSHARED const char* ThreadGetName(HOBJ hThread);
DLLSHARED uint32 ThreadGetId(void);
DLLSHARED const char* ThreadGetName(void);
DLLSHARED HOBJ ThreadCurrent(void);
DLLSHARED void ThreadSleep(uint32 Ms);
DLLSHARED void ThreadEnumerate(ThreadEnumProc_t ThreadEnumProc);
DLLSHARED void ThreadSuspendAllExceptThis(void);
DLLSHARED void ThreadResumeAllExceptThis(void);
DLLSHARED int32 ThreadGetCoreNum(void);
DLLSHARED int32 ThreadGetProcessorNum(void);