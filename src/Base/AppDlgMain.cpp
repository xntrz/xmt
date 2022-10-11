#include "AppDlgMain.hpp"
#include "AppDlgAbout.hpp"
#include "AppDlgDbg.hpp"
#include "AppModule.hpp"
#include "AppRc.hpp"
#include "AppCtrl.hpp"

#include "Shared/Common/Dlg.hpp"


static const TCHAR* DLG_PROP_INSTANCE = TEXT("_AppDlgMainInst");


struct AppDlgMain_t
{
    HWND hDlg;
    HMENU hMenu;
    HWND hDbgWnd;
};


static void AppDlgMainSetHotkeyEnable(AppDlgMain_t* AppDlgMain, bool bSet)
{
#ifdef _DEBUG
    DlgSetHotkey(AppDlgMain->hDlg, bSet, VK_ESCAPE, 0);
    DlgSetHotkey(AppDlgMain->hDlg, bSet, VK_F1, 0);
    DlgSetHotkey(AppDlgMain->hDlg, bSet, VK_F2, 0);
    DlgSetHotkey(AppDlgMain->hDlg, bSet, VK_F3, 0);
#endif    
};


static void AppDlgMainInitializeLocale(AppDlgMain_t* AppDlgMain)
{
    if (AppDlgMain->hMenu)
    {
        DlgFreeMenu(AppDlgMain->hMenu);
        AppDlgMain->hMenu = 0;
    };

    AppDlgMain->hMenu = DlgLoadMenu(IDD_MAIN_MENU);
    ASSERT(AppDlgMain->hMenu);

    SetMenu(AppDlgMain->hDlg, AppDlgMain->hMenu);
};


static bool AppDlgMainOnInitialize(AppDlgMain_t* AppDlgMain, HWND hDlg)
{    
    AppDlgMain->hDlg = hDlg;

    DlgInitTitleIcon(AppDlgMain->hDlg, IDS_TITLE, IDI_APPICO);
    
    AppCtrlInitialize(GetDlgItem(hDlg, IDD_MAIN_TABCTRL), hDlg);
    AppCtrlUpdateText();
    
    AppDlgMainSetHotkeyEnable(AppDlgMain, true);
    AppDlgMainInitializeLocale(AppDlgMain);

#ifdef _DEBUG
    AppDlgMain->hDbgWnd = AppDlgDbgCreate(AppDlgMain->hDlg);
#endif    

	return true;
};


static void AppDlgMainOnTerminate(AppDlgMain_t* AppDlgMain)
{
    if (AppDlgMain->hMenu)
    {
        DlgFreeMenu(AppDlgMain->hMenu);
        AppDlgMain->hMenu = 0;
    };

    AppCtrlTerminatePhase2();
};


static void AppDlgMainOnNotify(AppDlgMain_t* AppDlgMain, int32 IdCtrl, LPNMHDR lpNMHDR)
{
    switch (lpNMHDR->idFrom)
    {
    case IDD_MAIN_TABCTRL:
        {
            switch (lpNMHDR->code)
            {
            case TCN_SELCHANGE:
                {
                    AppCtrlSelectChanged();
                }                
                break;

            case TCN_SELCHANGING:
                {
                    if (AppCtrlSelectPreChange())
                        SetWindowLongPtr(AppDlgMain->hDlg, DWLP_MSGRESULT, true);
                }
                break;
            };
        }
        break;
    };
};


static void AppDlgMainOnCommand(AppDlgMain_t* AppDlgMain, int32 IdCtrl, HWND hWndCtrl, uint32 NotifyCode)
{
    switch (IdCtrl)
    {
    case IDD_MAIN_MENU_ABOUT:
        {
            AppDlgAboutCreate(AppDlgMain->hDlg);
        }
        break;

    case IDD_MAIN_MENU_RELOAD:
        {
            ;
        }
        break;
    };
};


static void AppDlgMainOnLocaleChanged(AppDlgMain_t* AppDlgMain)
{
    AppCtrlUpdateText();
    AppDlgMainInitializeLocale(AppDlgMain);
};


static void AppDlgMainOnChildDlgBegin(AppDlgMain_t* AppDlgMain, int32 IdDlg, HMODULE hModule)
{
    if (IdDlg == IDD_ABOUT)
        AppDlgMainSetHotkeyEnable(AppDlgMain, false);
};


static void AppDlgMainOnChildDlgEnd(AppDlgMain_t* AppDlgMain, int32 IdDlg, HMODULE hModule)
{
    if (IdDlg == IDD_ABOUT)
        AppDlgMainSetHotkeyEnable(AppDlgMain, true);
};


static void AppDlgMainOnModuleRequestLock(AppDlgMain_t* AppDlgMain)
{
    EnableMenuItem(AppDlgMain->hMenu, IDD_MAIN_MENU_RELOAD, MF_DISABLED);
    DrawMenuBar(AppDlgMain->hDlg);
    
    AppCtrlLock();
};


static void AppDlgMainOnModuleRequestUnlock(AppDlgMain_t* AppDlgMain)
{
    AppCtrlUnlock();
    
    EnableMenuItem(AppDlgMain->hMenu, IDD_MAIN_MENU_RELOAD, MF_ENABLED);
    DrawMenuBar(AppDlgMain->hDlg);
};


static void AppDlgMainOnDropFile(AppDlgMain_t* AppDlgMain, HDROP hDrop)
{
    AppCtrlDropFiles(hDrop);
};


static void AppDlgMainOnHotkey(AppDlgMain_t* AppDlgMain, int32 VK)
{
#ifdef _DEBUG
    switch (VK)
    {
    case VK_ESCAPE:
        {
            PostMessage(AppDlgMain->hDlg, WM_CLOSE, 0, 0);
        }
        break;
        
    case VK_F1:
        {
            AppCtrlDbgMount(true);
        }
        break;
        
    case VK_F2:
        {
            AppCtrlDbgMount(false);
        }
        break;

    case VK_F3:
        {
			;
        }
        break;
    };
#endif
};


static INT_PTR CALLBACK AppDlgMainMsgProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    AppDlgMain_t* AppDlgMain = (AppDlgMain_t*)GetProp(hDlg, DLG_PROP_INSTANCE);
    
    switch (Msg)
    {
#ifdef _DEBUG            
    case WM_MOVING:
        {
            RECT* RcPositionNew = LPRECT(lParam);
            
            RECT RcPositionNow  = { 0 };
            GetWindowRect(hDlg, &RcPositionNow);
            
            RECT RcDeltaMove    = { 0 };
            RcDeltaMove.left    = (RcPositionNew->left - RcPositionNow.left);
            RcDeltaMove.right   = (RcPositionNew->right - RcPositionNow.right);
            RcDeltaMove.top     = (RcPositionNew->top - RcPositionNow.top);
            RcDeltaMove.bottom  = (RcPositionNew->bottom - RcPositionNow.bottom);

            AppDlgDbgOnMoving(&RcDeltaMove);
        }
        return true;
#endif

    case WM_DLGINIT:
        AppDlgMain = (AppDlgMain_t*)lParam;
        SetProp(hDlg, DLG_PROP_INSTANCE, AppDlgMain);
        return AppDlgMainOnInitialize(AppDlgMain, hDlg);

    case WM_DLGTERM:
        AppDlgMainOnTerminate(AppDlgMain);
        return true;

    case WM_CLOSE:
        DlgEnd(hDlg);
        AppCtrlTerminatePhase1();
        return true;

    case WM_NOTIFY:
        AppDlgMainOnNotify(AppDlgMain, int32(wParam), LPNMHDR(lParam));
        return true;

    case WM_COMMAND:
        AppDlgMainOnCommand(AppDlgMain, LOWORD(wParam), HWND(lParam), HIWORD(wParam));
        return true;

    case WM_DLGLOCALE:
        AppDlgMainOnLocaleChanged(AppDlgMain);
        return true;

    case WM_DLGBEGIN:
        AppDlgMainOnChildDlgBegin(AppDlgMain, wParam, HMODULE(lParam));
        return true;

    case WM_DLGEND:
        AppDlgMainOnChildDlgEnd(AppDlgMain, wParam, HMODULE(lParam));
        return true;

    case WM_MODULE_REQUEST_LOCK:
        AppDlgMainOnModuleRequestLock(AppDlgMain);
        return true;

    case WM_MODULE_REQUEST_UNLOCK:
        AppDlgMainOnModuleRequestUnlock(AppDlgMain);
        return true;

    case WM_DROPFILES:
        AppDlgMainOnDropFile(AppDlgMain, HDROP(wParam));
        return true;

    case WM_HOTKEY:
        AppDlgMainOnHotkey(AppDlgMain, int32(wParam));
        return true;
    };

    return false;
};


void AppDlgMainCreate(void)
{
    AppDlgMain_t AppDlgMain = {};
    DlgBeginModal(AppDlgMainMsgProc, IDD_MAIN, NULL, &AppDlgMain);    
};