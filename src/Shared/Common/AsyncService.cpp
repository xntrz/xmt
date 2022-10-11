#include "AsyncService.hpp"
#include "Thread.hpp"
#include "Event.hpp"


static const uint32 DEFAULT_SLEEP_TIME = 20;


struct AsyncSvc_t
{
	AsyncSvcCallbacks_t Callbacks;
	HOBJ ThreadHandle;
	HOBJ EventHandle;
	uint32 SleepTime;
	void* UserParam;
	char szLabel[64];
	bool PauseFlag;
	bool RunFlag;
};


static void AsyncSvcWorkProc(void* Param)
{
	AsyncSvc_t* AsyncSvc = (AsyncSvc_t*)Param;
	ASSERT(AsyncSvc);

	if(AsyncSvc->Callbacks.Start)
		AsyncSvc->Callbacks.Start(AsyncSvc->UserParam);
	
	do
	{		
		if (!AsyncSvc->PauseFlag)
			AsyncSvc->Callbacks.Run(AsyncSvc->UserParam);		
	} while (!EventWaitFor(AsyncSvc->EventHandle, AsyncSvc->SleepTime));

	if (AsyncSvc->Callbacks.Stop)
		AsyncSvc->Callbacks.Stop(AsyncSvc->UserParam);
};


HOBJ AsyncSvcCreate(const char* pszLabel, const AsyncSvcCallbacks_t* Callbacks, uint32 SleepTime, void* Param)
{
	ASSERT(pszLabel);
	ASSERT(Callbacks);
	ASSERT(std::strlen(pszLabel) < COUNT_OF(AsyncSvc_t::szLabel));

	AsyncSvc_t* AsyncSvc = new AsyncSvc_t();
	if (AsyncSvc)
	{
		std::strcpy(AsyncSvc->szLabel, pszLabel);
		AsyncSvc->Callbacks = *Callbacks;
		AsyncSvc->SleepTime = SleepTime;
		AsyncSvc->UserParam = Param;
		AsyncSvc->ThreadHandle = 0;
		AsyncSvc->EventHandle = 0;
		AsyncSvc->RunFlag = false;
		AsyncSvc->PauseFlag = false;
	};

	return AsyncSvc;
};


void AsyncSvcDestroy(HOBJ hAsyncSvc)
{
	AsyncSvc_t* AsyncSvc = (AsyncSvc_t*)hAsyncSvc;
	ASSERT(AsyncSvc);
	ASSERT(AsyncSvc->RunFlag == false);

	if (AsyncSvc->ThreadHandle)
	{
		ThreadDestroy(AsyncSvc->ThreadHandle);
		AsyncSvc->ThreadHandle = 0;
	};

	if (AsyncSvc->EventHandle)
	{
		EventDestroy(AsyncSvc->EventHandle);
		AsyncSvc->EventHandle = 0;
	};

	delete AsyncSvc;
};


void AsyncSvcStart(HOBJ hAsyncSvc)
{
	AsyncSvc_t* AsyncSvc = (AsyncSvc_t*)hAsyncSvc;
	ASSERT(AsyncSvc);

	if (!AsyncSvc->RunFlag)
	{
		AsyncSvc->RunFlag = true;

		ASSERT(AsyncSvc->ThreadHandle == NULL);
		ASSERT(AsyncSvc->EventHandle == NULL);
		
		AsyncSvc->EventHandle = EventCreate();
		ASSERT(AsyncSvc->EventHandle != NULL);

		AsyncSvc->ThreadHandle = ThreadCreate(AsyncSvc->szLabel, AsyncSvcWorkProc, 0, AsyncSvc);
		ASSERT(AsyncSvc->ThreadHandle != NULL);
	};
};


void AsyncSvcStop(HOBJ hAsyncSvc)
{
	AsyncSvc_t* AsyncSvc = (AsyncSvc_t*)hAsyncSvc;
	ASSERT(AsyncSvc);

	if (AsyncSvc->RunFlag)
	{
		AsyncSvc->RunFlag = false;

		ASSERT(AsyncSvc->ThreadHandle != NULL);
		ASSERT(AsyncSvc->EventHandle != NULL);

		EventSignalAll(AsyncSvc->EventHandle);
		ThreadWait(AsyncSvc->ThreadHandle);

		ThreadDestroy(AsyncSvc->ThreadHandle);
		EventDestroy(AsyncSvc->EventHandle);

		AsyncSvc->ThreadHandle = NULL;
		AsyncSvc->EventHandle = NULL;
	};
};


void AsyncSvcPause(HOBJ hAsyncSvc)
{
	AsyncSvc_t* AsyncSvc = (AsyncSvc_t*)hAsyncSvc;
	ASSERT(AsyncSvc);

	AsyncSvc->PauseFlag = true;
};


void AsyncSvcResume(HOBJ hAsyncSvc)
{
	AsyncSvc_t* AsyncSvc = (AsyncSvc_t*)hAsyncSvc;
	ASSERT(AsyncSvc);

	AsyncSvc->PauseFlag = false;
};


bool AsyncSvcIsRunning(HOBJ hAsyncSvc)
{
	AsyncSvc_t* AsyncSvc = (AsyncSvc_t*)hAsyncSvc;
	ASSERT(AsyncSvc);

	return AsyncSvc->RunFlag;
};