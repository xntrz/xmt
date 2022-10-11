#pragma once

struct AsyncSvcCallbacks_t
{
	void(*Start)(void* Param);
	void(*Stop)(void* Param);
	void(*Run)(void* Param);
};


DLLSHARED HOBJ AsyncSvcCreate(const char* pszLabel, const AsyncSvcCallbacks_t* Callbacks, uint32 SleepTime, void* Param = nullptr);
DLLSHARED void AsyncSvcDestroy(HOBJ hAsyncSvc);
DLLSHARED void AsyncSvcStart(HOBJ hAsyncSvc);
DLLSHARED void AsyncSvcStop(HOBJ hAsyncSvc);
DLLSHARED void AsyncSvcPause(HOBJ hAsyncSvc);
DLLSHARED void AsyncSvcResume(HOBJ hAsyncSvc);
DLLSHARED bool AsyncSvcIsRunning(HOBJ hAsyncSvc);