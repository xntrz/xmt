#pragma once

typedef bool(*TcpIoSvcCompletionProc_t)(void* Act, uint32 Bytes, uint32 ErrorCode);

void TcpIoSvcInitialize(void);
void TcpIoSvcTerminate(void);
void TcpIoSvcWaitForActEnd(void);
void TcpIoSvcRefInc(void);
void TcpIoSvcRefDec(void);
bool TcpIoSvcRegist(HANDLE hSocket, TcpIoSvcCompletionProc_t CompletionProc);
void TcpIoSvcStats(int32* IoNum, int32* ThreadsNum, int32* ThreadsAwait);