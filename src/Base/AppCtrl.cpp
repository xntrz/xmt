#include "AppCtrl.hpp"
#include "AppModule.hpp"
#include "AppSettings.hpp"

#include "Shared/Common/Dlg.hpp"
#include "Shared/Common/Registry.hpp"


struct AppCtrl_t
{
    HWND hWnd;
    HWND hWndParent;
    HWND ahWndTab[MODULE_MAX];
    int32 TabNum;
    int32 TabCurrent;
    HIMAGELIST hImageList;
    HOBJ hRedirect;
    std::atomic<bool> LockFlag;
    std::atomic<bool> ReloadFlag;
#ifdef _DEBUG
    std::atomic<bool> DbgMountFlag;
#endif    
};


static AppCtrl_t AppCtrl;


//
//  Returned icon should be closed via DestroyIcon
//
static HICON AppCtrlGetModuleDlgIcon(HWND hWnd)
{
    uint32 Timeout = 1000;
    HICON hResult = NULL;

    SendMessageTimeout(hWnd, WM_GETICON, ICON_SMALL, 0, 0, Timeout, PDWORD_PTR(&hResult));

    if (hResult == NULL)
        SendMessageTimeout(hWnd, WM_GETICON, ICON_SMALL2, 0, 0, Timeout, PDWORD_PTR(&hResult));

    if (hResult == NULL)
        SendMessageTimeout(hWnd, WM_GETICON, ICON_BIG, 0, 0, Timeout, PDWORD_PTR(&hResult));

    if (hResult == NULL)
        hResult = HICON(GetClassLongPtr(hWnd, GCLP_HICON));

    if (hResult == NULL)
        hResult = HICON(GetClassLongPtr(hWnd, GCLP_HICONSM));

    if (hResult == NULL)
        hResult = LoadIcon(NULL, IDI_APPLICATION);

    if (hResult)
        hResult = CopyIcon(hResult);

    return hResult;
};


static int32 AppCtrlGetTabIndexByModuleNo(int32 ModuleNo)
{
    ASSERT(ModuleNo >= 0 && ModuleNo < AppModGetCount());
    
    for (int32 i = 0; i < AppCtrl.TabNum; ++i)
    {
        TCITEM TcItem = {};
        TcItem.mask = TCIF_PARAM;
        
        TabCtrl_GetItem(AppCtrl.hWnd, i, &TcItem);
        
        if (int32(TcItem.lParam) == ModuleNo)
            return i;
    };

    return -1;
};


void AppCtrlInitialize(HWND hWndTab, HWND hWndDlg)
{
    AppModInitialize();

    AppCtrl.hWnd = hWndTab;
    AppCtrl.hWndParent = hWndDlg;
    ASSERT(hWndDlg == GetParent(hWndTab));

    if (!AppCtrl.ReloadFlag)
    {
        AppCtrl.hRedirect = DlgRedirectCreate(hWndTab, hWndDlg);
        ASSERT(AppCtrl.hRedirect);
        DlgRedirectRegist(AppCtrl.hRedirect, WM_DLGBEGIN);
        DlgRedirectRegist(AppCtrl.hRedirect, WM_DLGEND);
        DlgRedirectRegist(AppCtrl.hRedirect, WM_MODULE_REQUEST_LOCK);
        DlgRedirectRegist(AppCtrl.hRedirect, WM_MODULE_REQUEST_UNLOCK);
    };
    
    AppCtrl.hImageList = ImageList_Create(12, 12, ILC_COLOR32, 0, COUNT_OF(AppCtrl.ahWndTab));
    ASSERT(AppCtrl.hImageList);
    TabCtrl_SetImageList(AppCtrl.hWnd, AppCtrl.hImageList);
    
    AppCtrl.TabCurrent = -1;
    int32 ModuleCount = AppModGetCount();
    for (int32 i = 0; i < ModuleCount; ++i)
    {
        HWND hWndMod = AppModGetDialog(i, AppCtrl.hWnd, MODULE_DLGTYPE_MAIN);
        if (hWndMod == NULL)
            continue;

		int32 Index = AppCtrl.TabNum++;

        ShowWindow(hWndMod, SW_HIDE);

        HICON hIcon = AppCtrlGetModuleDlgIcon(hWndMod);
        if (hIcon)
        {
            ImageList_AddIcon(AppCtrl.hImageList, hIcon);
            DestroyIcon(hIcon);
        };

        TCITEM TcItem = {};
        TcItem.mask = TCIF_TEXT | TCIF_PARAM | TCIF_IMAGE;
        TcItem.pszText = TEXT("");
        TcItem.lParam = LPARAM(i);
        TcItem.iImage = Index;
        TabCtrl_InsertItem(AppCtrl.hWnd, Index, &TcItem);

        RECT RcClient = { 0 };
        GetClientRect(AppCtrl.hWnd, &RcClient);
        TabCtrl_AdjustRect(AppCtrl.hWnd, FALSE, &RcClient);

        MoveWindow(
            hWndMod,
            RcClient.left,
            RcClient.top,
            RcClient.right - RcClient.left,
            RcClient.bottom - RcClient.top,
            TRUE
        );

        InvalidateRect(hWndMod, &RcClient, TRUE);

        AppCtrl.ahWndTab[Index] = hWndMod;
    };

#ifdef _DEBUG
    AppCtrl.DbgMountFlag = true;
#endif

    for (int32 i = 0; i < AppCtrl.TabNum; ++i)
    {
        TCITEM TcItem = {};
        TcItem.mask = TCIF_PARAM;        
        TabCtrl_GetItem(AppCtrl.hWnd, i, &TcItem);
    };

	if (AppSettings.ModCurSel[0] != '\0')
	{
		int32 ModNo = AppModGetNo(AppSettings.ModCurSel);
		int32 TabNo = AppCtrlGetTabIndexByModuleNo(ModNo);
		if (TabNo != -1)
			AppCtrlSetSelect(TabNo);
		else
			AppCtrlSetSelect(0);
	}
	else
	{
		AppCtrlSetSelect(0);
	};
};


void AppCtrlTerminatePhase1(void)
{
    int32 DeletedItemNum = 0;
    
    for (int32 i = 0; i < AppCtrl.TabNum; ++i)
    {
		DlgEnd(AppCtrl.ahWndTab[i]);
		AppCtrl.ahWndTab[i] = NULL;

        int32 ItemIndex = (i - DeletedItemNum);
        
        TabCtrl_DeleteItem(AppCtrl.hWnd, ItemIndex);
		++DeletedItemNum;
    };

	AppCtrl.TabNum = 0;
};


void AppCtrlTerminatePhase2(void)
{
    if (AppCtrl.hImageList)
    {
        TabCtrl_SetImageList(AppCtrl.hWnd, NULL);
        ImageList_Destroy(AppCtrl.hImageList);
        AppCtrl.hImageList = NULL;
    };

    if (!AppCtrl.ReloadFlag)
    {
        if (AppCtrl.hRedirect)
        {
            DlgRedirectDestroy(AppCtrl.hRedirect);
            AppCtrl.hRedirect = 0;
        };
    };    

    AppModTerminate();
};


void AppCtrlSelectChanged(void)
{
    if (AppCtrl.TabCurrent != -1)
    {
        ShowWindow(AppCtrl.ahWndTab[AppCtrl.TabCurrent], SW_HIDE);
        SendMessage(AppCtrl.ahWndTab[AppCtrl.TabCurrent], WM_MODULE_DETACH, 0, 0);
    };
    
    AppCtrl.TabCurrent = AppCtrlGetSelect();

    TCITEM TcItem = {};
    TcItem.mask = TCIF_PARAM;    
    TabCtrl_GetItem(AppCtrl.hWnd, AppCtrl.TabCurrent, &TcItem);    
    int32 ModCurrent = TcItem.lParam;
    
    AppModSetCurrent(ModCurrent);

    const char* Label = AppModGetLabel(ModCurrent);
	if (Label)
	{
		if (std::strlen(Label) < COUNT_OF(AppSettings.ModCurSel))
			std::strcpy(&AppSettings.ModCurSel[0], Label);
	};

    if (AppCtrl.TabCurrent != -1)
    {
        ShowWindow(AppCtrl.ahWndTab[AppCtrl.TabCurrent], SW_SHOW);
        SendMessage(AppCtrl.ahWndTab[AppCtrl.TabCurrent], WM_MODULE_ATTACH, 0, 0);
    };
};


bool AppCtrlSelectPreChange(void)
{
    //
    //  Returns true to disallow tab page change, false otherwise
    //
    return (AppCtrl.LockFlag);
};


void AppCtrlDropFiles(HDROP hDrop)
{
    SendMessage(AppCtrl.ahWndTab[AppCtrl.TabCurrent], WM_DROPFILES, WPARAM(hDrop), 0);
};


void AppCtrlUpdateText(void)
{
    int32 ItemCount = TabCtrl_GetItemCount(AppCtrl.hWnd);
    ASSERT(ItemCount == AppCtrl.TabNum);
    for (int32 i = 0; i < ItemCount; ++i)
    {
        TCITEM tcItem = {};
        tcItem.mask = TCIF_PARAM;
        TabCtrl_GetItem(AppCtrl.hWnd, i, &tcItem);

        TCHAR tszTextBuff[TEXT_BUFF_SIZE];
        tszTextBuff[0] = TEXT('\0');
        int32 Len = GetWindowText(AppCtrl.ahWndTab[i], tszTextBuff, COUNT_OF(tszTextBuff));

        if (Len > 0)
        {
            tcItem.mask = TCIF_TEXT;
            tcItem.pszText = tszTextBuff;
            tcItem.cchTextMax = Len;

            TabCtrl_SetItem(AppCtrl.hWnd, i, &tcItem);
        };

    };
    
    InvalidateRect(AppCtrl.hWnd, NULL, TRUE);
};


void AppCtrlLock(void)
{
    ASSERT(!AppCtrl.LockFlag);
    
    AppCtrl.LockFlag = true;
    AppModLock();
};


void AppCtrlUnlock(void)
{
    ASSERT(AppCtrl.LockFlag);
    
    AppModUnlock();
    AppCtrl.LockFlag = false;
};


void AppCtrlSetSelect(int32 No)
{
    if (No >= 0 && No < AppCtrl.TabNum)
    {
        TabCtrl_SetCurSel(AppCtrl.hWnd, No);
        AppCtrlSelectChanged();
    };
};


int32 AppCtrlGetSelect(void)
{
    return TabCtrl_GetCurSel(AppCtrl.hWnd);
};


void AppCtrlDbgMount(bool MountUnmountFlag)
{
    //
    //  true - mount all
    //  false - unmount all
    //

    if (AppCtrl.LockFlag)
    {
        OUTPUTLN("Can't mount/unmount while module is locked");
        return;
    };

#ifdef _DEBUG    
    AppCtrl.ReloadFlag = true;
    if (MountUnmountFlag)
    {
        if (!AppCtrl.DbgMountFlag)
        {
            AppCtrl.DbgMountFlag = true;
            AppCtrlInitialize(AppCtrl.hWnd, AppCtrl.hWndParent);
            AppCtrlSelectChanged();
            AppCtrlUpdateText();
            OUTPUTLN("Mount all modules OK!");
        };
    }
    else
    {
        if (AppCtrl.DbgMountFlag)
        {
			AppCtrl.DbgMountFlag = false;
            AppCtrlTerminatePhase1();
            AppCtrlTerminatePhase2();
            OUTPUTLN("Unmount all modules OK!");
        };
    };
    AppCtrl.ReloadFlag = false;
#endif    
};