#pragma once

#define TEXT_BUFF_SIZE      (256)

#define WM_DLGINIT          (WM_USERMSG + 1)
#define WM_DLGTERM          (WM_USERMSG + 2)
#define WM_DLGBEGIN         (WM_USERMSG + 3)
#define WM_DLGEND           (WM_USERMSG + 4)
#define WM_DLGLOCALE        (WM_USERMSG + 5)

#define WM_DLGPRIVATE       (WM_USERMSG + 50)
#define WM_APPMSG           (WM_USERMSG + 100)

enum DlgLocale_t
{
    DlgLocale_Russian = 0,
    DlgLocale_English,
    DlgLocale_SystemDefault = -1,
    DlgLocale_Default = DlgLocale_English,
};

enum DlgResizeFlag_t
{
    DlgResizeFlag_X     = BIT(0),
    DlgResizeFlag_Y     = BIT(1),
    DlgResizeFlag_CX    = BIT(2),
    DlgResizeFlag_CY    = BIT(3),
    DlgResizeFlag_Move  = DlgResizeFlag_X | DlgResizeFlag_Y,
    DlgResizeFlag_Size  = DlgResizeFlag_CX | DlgResizeFlag_CY,
};

enum DlgTbProgressState_t
{
    DlgTbProgressState_NoProgress = 0,
    DlgTbProgressState_Intermediate,
    DlgTbProgressState_Normal,
    DlgTbProgressState_Pause,
    DlgTbProgressState_Error,
};

DLLSHARED bool DlgInitialize(void);
DLLSHARED void DlgTerminate(void);
DLLSHARED void DlgLocaleSet(DlgLocale_t Locale);
DLLSHARED HINSTANCE DlgSetCurrentModule(HINSTANCE hInstance);
DLLSHARED int32 DlgBeginModal(DLGPROC MsgProc, HMODULE hModule, int32 Id, HWND hWndParent = NULL, void* Param = nullptr);
DLLSHARED int32 DlgBeginModal(DLGPROC MsgProc, int32 Id, HWND hWndParent = NULL, void* Param = nullptr);
DLLSHARED HWND DlgBeginModeless(DLGPROC MsgProc, HMODULE hModule, int32 Id, HWND hWndParent = NULL, void* Param = nullptr);
DLLSHARED HWND DlgBeginModeless(DLGPROC MsgProc, int32 Id, HWND hWndParent = NULL, void* Param = nullptr);
DLLSHARED int32 DlgRun(HWND hDlg);
DLLSHARED void DlgEnd(HWND hDlg, int32 Result = -1);
DLLSHARED HMODULE DlgGetModule(HWND hWnd);
DLLSHARED void DlgSetHotkey(HWND hDlg, bool SetFlag, int32 VK, int32 Mod, bool FocusFlag = true);
DLLSHARED HMENU DlgLoadMenu(int32 IdMenu);
DLLSHARED void DlgFreeMenu(HMENU hMenu);
DLLSHARED HICON DlgLoadIcon(int32 IdIcon, int32 cx, int32 cy);
DLLSHARED void DlgFreeIcon(HICON hIcon);
DLLSHARED std::tstring DlgLoadText(int32 IdText);
DLLSHARED bool DlgLoadText(int32 IdText, TCHAR* Buffer, int32 BufferSize);
DLLSHARED void DlgLoadCtrlTextA(HWND hDlg, int32 IdCtrl, const char* Buffer);
DLLSHARED void DlgLoadCtrlTextW(HWND hDlg, int32 IdCtrl, const wchar_t* Buffer);
DLLSHARED void DlgLoadCtrlTextId(HWND hDlg, int32 IdCtrl, int32 IdText);
DLLSHARED int32 DlgShowErrorMsg(HWND hWnd, int32 idText, int32 idTitle);
DLLSHARED int32 DlgShowAlertMsg(HWND hWnd, int32 idText, int32 idTitle);
DLLSHARED int32 DlgShowNotifyMsg(HWND hWnd, int32 idText, int32 idTitle);
DLLSHARED void DlgAttachTimer(HWND hWnd, int32 idTimer, uint32 intervalMS, TIMERPROC pfnCallback = nullptr);
DLLSHARED void DlgDetachTimer(HWND hWnd, int32 idTimer);
DLLSHARED RECT DlgGetLocalPoint(HWND hWnd);
DLLSHARED bool DlgGetOpenFilename(HWND hWndParent, TCHAR* buff, int32 buffSize, TCHAR* filter);
DLLSHARED bool DlgGetOpenFilenameEx(HWND hWndParent, TCHAR ppBuff[][MAX_PATH], int32* buffc, TCHAR* filter);
DLLSHARED bool DlgGetSaveFilename(HWND hWndParent, TCHAR* buff, int32 buffSize, TCHAR* filter, TCHAR* pszDefaultExtension = nullptr);
DLLSHARED void DlgInitTitleIcon(HWND hDlg, int32 IdText, int32 IdIcon);
DLLSHARED HOBJ DlgResizeCreate(HWND hWndTarget);
DLLSHARED void DlgResizeDestroy(HOBJ hResize);
DLLSHARED void DlgResizeRegist(HOBJ hResize, HWND hWnd, DlgResizeFlag_t Flags);
DLLSHARED void DlgResizeRegistEx(HOBJ hResize, int32 IdCtrl, DlgResizeFlag_t Flags);
DLLSHARED void DlgResizeRemove(HOBJ hResize, HWND hWnd);
DLLSHARED HOBJ DlgRedirectCreate(HWND hWndFrom, HWND hWndTo);
DLLSHARED void DlgRedirectDestroy(HOBJ hRedirect);
DLLSHARED void DlgRedirectRegist(HOBJ hRedirect, uint32 Msg);
DLLSHARED HOBJ DlgFDropBegin(HDROP hDrop);
DLLSHARED void DlgFDropEnd(HOBJ hFDrop);
DLLSHARED int32 DlgFDropGetNum(HOBJ hFDrop);
DLLSHARED const TCHAR* DlgFDropGetPath(HOBJ hFDrop, int32 Index);
DLLSHARED bool DlgTaskbarProgressSetState(HWND hWnd, DlgTbProgressState_t State);
DLLSHARED bool DlgTaskbarProgressSetValue(HWND hWnd, int32 Current, int32 Max);

#ifdef _UNICODE
#define DlgLoadCtrlText DlgLoadCtrlTextW
#else
#define DlgLoadCtrlText DlgLoadCtrlTextA
#endif