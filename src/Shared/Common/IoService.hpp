#pragma once

struct IoSvcCallbacks_t
{
	void(*IoThreadStart)(void* Param);
	void(*IoThreadStop)(void* Param);
	bool(*IoComplete)(uint32 ErrorCode, uint32 Bytes, void* CompletionKey, void* Act, void* Param);
};


DLLSHARED HOBJ IoSvcCreate(const char* Label, int32 ThreadCount, const IoSvcCallbacks_t* Callbacks, void* Param = nullptr);
DLLSHARED void IoSvcDestroy(HOBJ hIoSvc);
DLLSHARED bool IoSvcBind(HOBJ hIoSvc, void* Handle, void* CompletionKey = nullptr);
DLLSHARED bool IoSvcPost(HOBJ hIoSvc, uint32 bytes = 0, void* CompletionKey = nullptr, void* Act = nullptr);
DLLSHARED int32 IoSvcThreadsAvailable(HOBJ hIoSvc);