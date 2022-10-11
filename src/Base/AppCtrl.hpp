#pragma once


void AppCtrlInitialize(HWND hWndTab, HWND hWndDlg);
void AppCtrlTerminatePhase1(void);
void AppCtrlTerminatePhase2(void);
void AppCtrlSelectChanged(void);
bool AppCtrlSelectPreChange(void);
void AppCtrlDropFiles(HDROP hDrop);
void AppCtrlUpdateText(void);
void AppCtrlLock(void);
void AppCtrlUnlock(void);
void AppCtrlSetSelect(int32 No);
int32 AppCtrlGetSelect(void);
void AppCtrlDbgMount(bool MountUnmountFlag);