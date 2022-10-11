#pragma once

void AppModInitialize(void);
void AppModTerminate(void);
void AppModSetCurrent(int32 No);
void AppModLock(void);
void AppModUnlock(void);
HWND AppModGetDialog(int32 No, HWND hWndParent, int32 Type);
HINSTANCE AppModGetInstance(int32 No);
int32 AppModGetCount(void);
int32 AppModGetNo(const char* Label);
const char* AppModGetLabel(int32 No);