#include "IoService.hpp"
#include "Thread.hpp"
#include "Time.hpp"

#if WINVER >= _WIN32_WINNT_VISTA
//
//	RtlNtStatusToDosError
//
#include <winternl.h>
#pragma comment(lib, "Ntdll.lib")
#endif


struct IoSvc_t
{
	HANDLE PortHandle;
	HOBJ* ThreadArray;
	std::atomic<int32> ThreadCount;
	std::atomic<int32> ThreadsAlive;
	std::atomic<int32> ThreadAvailable;
	IoSvcCallbacks_t Callbacks;
	void* UserParam;
	bool RunFlag;	
};


static void IoSvcWake(IoSvc_t* IoSvc)
{
	if (IoSvc->ThreadsAlive > 0)
		IoSvcPost(IoSvc, 0);
};


static void IoSvcWorkProc(void* Param)
{
	IoSvc_t* IoSvc = (IoSvc_t*)Param;
	ASSERT(IoSvc);

	++IoSvc->ThreadsAlive;

	if (IoSvc->Callbacks.IoThreadStart)
		IoSvc->Callbacks.IoThreadStart(IoSvc->UserParam);

	while (true)
	{
#if WINVER >= _WIN32_WINNT_VISTA
		uint32 ErrorCode = 0;
		uint32 NumEntries = 0;
		OVERLAPPED_ENTRY aOverlappedEntries[64];
		std::memset(aOverlappedEntries, 0x00, sizeof(aOverlappedEntries));

		++IoSvc->ThreadAvailable;
		bool bResult = bool(GetQueuedCompletionStatusEx(IoSvc->PortHandle, aOverlappedEntries, _countof(aOverlappedEntries), PULONG(&NumEntries), INFINITE, FALSE) > 0);
		--IoSvc->ThreadAvailable;
		if (!bResult)
		{
			ErrorCode = GetLastError();
			ASSERT(false);
		};

		if (!IoSvc->RunFlag)
			break;

		for (uint32 i = 0; i < NumEntries; ++i)
		{
			uint32 CompletionErrorCode 	= RtlNtStatusToDosError(aOverlappedEntries[i].Internal);
			uint32 NumBytes 			= uint32(aOverlappedEntries[i].dwNumberOfBytesTransferred);
			void* CompletionKey 		= (void*)aOverlappedEntries[i].lpCompletionKey;
			OVERLAPPED* Overlapped 		= aOverlappedEntries[i].lpOverlapped;

			IoSvc->Callbacks.IoComplete(
				CompletionErrorCode,
				NumBytes,
				CompletionKey,
				Overlapped,
				IoSvc->UserParam
			);
		};
#else
		uint32 ErrorCode 		= 0;
		uint32 NumBytes 		= 0;
		void* CompletionKey 	= nullptr;
		OVERLAPPED* Overlapped 	= nullptr;

		++IoSvc->ThreadAvailable;
		bool bResult = bool(GetQueuedCompletionStatus(IoSvc->PortHandle, LPDWORD(&NumBytes), PULONG_PTR(&CompletionKey), &Overlapped, INFINITE) > 0);
		--IoSvc->ThreadAvailable;
		if (!bResult)
		{
			if (Overlapped)
			{
				ErrorCode = GetLastError();
			}
			else
			{
				ErrorCode = GetLastError();
				ASSERT(false);
			};
		};

		if(!IoSvc->RunFlag)
			break;

		IoSvc->Callbacks.IoComplete(
			ErrorCode,
			NumBytes,
			CompletionKey,
			Overlapped,
			IoSvc->UserParam
		);
#endif
	};

	if (IoSvc->Callbacks.IoThreadStart)
		IoSvc->Callbacks.IoThreadStop(IoSvc->UserParam);

	--IoSvc->ThreadsAlive;	

	IoSvcWake(IoSvc);
};


HOBJ IoSvcCreate(const char* Label, int32 ThreadCount, const IoSvcCallbacks_t* Callbacks, void* Param)
{
	ASSERT(ThreadCount > 0);
	ASSERT(Callbacks);
	ASSERT(Callbacks->IoComplete);

	IoSvc_t* IoSvc = new IoSvc_t();
	if (!IoSvc)
		return 0;

	IoSvc->PortHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, NULL);
	if (IoSvc->PortHandle != NULL)
	{
		IoSvc->ThreadCount = ThreadCount;
		IoSvc->ThreadsAlive = 0;
		IoSvc->ThreadAvailable = 0;
		IoSvc->Callbacks = *Callbacks;
		IoSvc->UserParam = Param;
		IoSvc->RunFlag = true;
		IoSvc->ThreadArray = new HOBJ[IoSvc->ThreadCount]();

		for (int32 i = 0; i < IoSvc->ThreadCount; ++i)
		{
			IoSvc->ThreadArray[i] = ThreadCreate(std::string(Label + std::to_string(i)).c_str(), IoSvcWorkProc, 0, IoSvc);
			ASSERT(IoSvc->ThreadArray[i]);
		};
	}
	else
	{
		delete IoSvc;
		IoSvc = 0;
	};

	return HOBJ(IoSvc);
};


void IoSvcDestroy(HOBJ hIoSvc)
{
	IoSvc_t* IoSvc = (IoSvc_t*)hIoSvc;
	ASSERT(IoSvc);
	
	if (IoSvc->RunFlag)
	{
		IoSvc->RunFlag = false;
		IoSvcWake(IoSvc);
	};

	if (IoSvc->ThreadCount)
	{
		ASSERT(IoSvc->ThreadArray);

		for (int32 i = 0; i < IoSvc->ThreadCount; ++i)
		{
			ASSERT(IoSvc->ThreadArray[i]);
			ThreadWait(IoSvc->ThreadArray[i]);
			ThreadDestroy(IoSvc->ThreadArray[i]);
			IoSvc->ThreadArray[i] = 0;
		};

		IoSvc->ThreadCount = 0;

		delete[] IoSvc->ThreadArray;
		IoSvc->ThreadArray = nullptr;
	};

	if (IoSvc->PortHandle != NULL)
	{
		CloseHandle(IoSvc->PortHandle);
		IoSvc->PortHandle = NULL;
	};

	delete IoSvc;
};


bool IoSvcBind(HOBJ hIoSvc, void* Handle, void* CompletionKey)
{
	IoSvc_t* IoSvc = (IoSvc_t*)hIoSvc;
	ASSERT(IoSvc);
	
	return bool(CreateIoCompletionPort(Handle, IoSvc->PortHandle, ULONG_PTR(CompletionKey), NULL) != NULL);
};


bool IoSvcPost(HOBJ hIoSvc, uint32 Bytes, void* CompletionKey, void* Act)
{
	IoSvc_t* IoSvc = (IoSvc_t*)hIoSvc;
	ASSERT(IoSvc);
	
	return bool(PostQueuedCompletionStatus(IoSvc->PortHandle, Bytes, ULONG_PTR(CompletionKey), LPOVERLAPPED(Act)) > 0);
};


int32 IoSvcThreadsAvailable(HOBJ hIoSvc)
{
	IoSvc_t* IoSvc = (IoSvc_t*)hIoSvc;
	ASSERT(IoSvc);

	return IoSvc->ThreadAvailable;
};