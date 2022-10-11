#include "BngDlg.hpp"
#include "BngRc.hpp"
#include "BngObj.hpp"
#include "BngResult.hpp"
#include "BngSettings.hpp"

#include "Shared/Network/NetProxy.hpp"
#include "Shared/Common/Time.hpp"
#include "Shared/Common/Dlg.hpp"


static const int32 IDT_RESULT_UPDATE = 100;
static const int32 IDT_RESULT_EOL_CHK = 101;
static const uint32 TIMER_PERIOD_INTERVAL = 50;
static const TCHAR* DLG_PROP_INSTANCE = TEXT("_BngDlgInst");


struct BngDlg_t
{
    HINSTANCE hInstance;
    HWND hDlg;
    HWND hEditVwCtrl;
    bool FlagRun;
};


static void BngDlgUpdateResult(BngDlg_t* BngDlg)
{
    std::tstring tszStatsuNone = DlgLoadText(IDS_STATUS_NONE);
    std::tstring tszStatsuRun = DlgLoadText(IDS_STATUS_RUN);

    if (!BngDlg->FlagRun)
    {
        bool OK = int32(BngSettings.TargetId[0] != '\0');

        if (OK && BngSettings.ProxyUseMode)
            OK = bool(CBngResult::Instance().ImportList().count() > 0);        

        EnableWindow(GetDlgItem(BngDlg->hDlg, IDD_BNG_START), OK ? TRUE : FALSE);
        EnableWindow(GetDlgItem(BngDlg->hDlg, IDD_BNG_STOP), FALSE);
        EnableWindow(GetDlgItem(BngDlg->hDlg, IDD_BNG_GRP_PARAM_CHANNEL), TRUE);
        EnableWindow(GetDlgItem(BngDlg->hDlg, IDD_BNG_GRP_PARAM_VIEWER), TRUE);
        EnableWindow(GetDlgItem(BngDlg->hDlg, IDD_BNG_GRP_PARAM_PRX), BngSettings.ProxyUseMode ? TRUE : FALSE);
        EnableWindow(GetDlgItem(BngDlg->hDlg, IDD_BNG_GRP_PARAM_PRXLIST), BngSettings.ProxyUseMode ? TRUE : FALSE);
        EnableWindow(GetDlgItem(BngDlg->hDlg, IDD_BNG_GRP_PARAM_PRXMODE), TRUE);
    }
    else
    {
        EnableWindow(GetDlgItem(BngDlg->hDlg, IDD_BNG_START), FALSE);
        EnableWindow(GetDlgItem(BngDlg->hDlg, IDD_BNG_STOP), TRUE);
        EnableWindow(GetDlgItem(BngDlg->hDlg, IDD_BNG_GRP_PARAM_CHANNEL), FALSE);
        EnableWindow(GetDlgItem(BngDlg->hDlg, IDD_BNG_GRP_PARAM_VIEWER), FALSE);
        EnableWindow(GetDlgItem(BngDlg->hDlg, IDD_BNG_GRP_PARAM_PRX), FALSE);
        EnableWindow(GetDlgItem(BngDlg->hDlg, IDD_BNG_GRP_PARAM_PRXLIST), FALSE);
        EnableWindow(GetDlgItem(BngDlg->hDlg, IDD_BNG_GRP_PARAM_PRXMODE), FALSE);
    };

    std::tstring tszStatsFormat = DlgLoadText(IDS_RESULT_FORMAT);
    TCHAR Buffer[TEXT_BUFF_SIZE * 4];
    _stprintf(
        Buffer,
        &tszStatsFormat[0],
        (BngDlg->FlagRun ? &tszStatsuRun[0] : &tszStatsuNone[0]),
        CBngResult::Instance().GetViewerCountWork(),
        CBngResult::Instance().GetViewerCountReal(),
        CBngResult::Instance().GetCtxObjWalkTime()
    );

    SetWindowText(GetDlgItem(BngDlg->hDlg, IDD_BNG_GRP_RESULT_VALUE), Buffer);
};


static void BngDlgCheckStart(BngDlg_t* BngDlg)
{
    SendMessage(GetParent(BngDlg->hDlg), WM_MODULE_REQUEST_LOCK, 0, 0);
    DlgAttachTimer(BngDlg->hDlg, IDT_RESULT_UPDATE, TIMER_PERIOD_INTERVAL);
    DlgAttachTimer(BngDlg->hDlg, IDT_RESULT_EOL_CHK, TIMER_PERIOD_INTERVAL);

    BngDlg->FlagRun = true;
    BngDlgUpdateResult(BngDlg);
};


static void BngDlgCheckStop(BngDlg_t* BngDlg, bool UserRequestFlag)
{
    BngDlg->FlagRun = false;
    BngDlgUpdateResult(BngDlg);

    DlgDetachTimer(BngDlg->hDlg, IDT_RESULT_UPDATE);
    DlgDetachTimer(BngDlg->hDlg, IDT_RESULT_EOL_CHK);
    SendMessage(GetParent(BngDlg->hDlg), WM_MODULE_REQUEST_UNLOCK, 0, 0);
    
    if (UserRequestFlag)
        return;

    //
    //  Handle error display request
    //
    CBngResult::ERRTYPE Errtype = CBngResult::Instance().GetError();
    if (Errtype != CBngResult::ERRTYPE_NONE)
    {
        //
        //  Exclude NONE error
        //
        int32 Index = (Errtype - 1);

        static const int32 anErrorTextId[] =
        {
            IDS_ERR_CHEXIST,
            IDS_ERR_CHLIVE,
            IDS_ERR_HOST_UNREACH,
            IDS_ERR_DETECT,
        };

        static_assert(COUNT_OF(anErrorTextId) == (CBngResult::ERRTYPENUM - 1), "update me");
        ASSERT(Index >= 0 && Index < COUNT_OF(anErrorTextId));

        DlgShowErrorMsg(BngDlg->hDlg, anErrorTextId[Index], IDS_ERROR);
    };
};


static void BngDlgImport(BngDlg_t* BngDlg, const char* Path)
{
    CBngResult::Instance().ImportList().clear();

    if (!CBngResult::Instance().ImportList().open(Path))
        DlgShowErrorMsg(BngDlg->hDlg, IDS_ERR_IMPORT, IDS_ERROR);

    BngDlgUpdateResult(BngDlg);
};


static void BngDlgCheckboxChanged(BngDlg_t* BngDlg, int32 Id)
{
    switch (Id)
    {
    case IDD_BNG_GRP_PARAM_PRXMODE:
        {
            HWND hWndCheck = GetDlgItem(BngDlg->hDlg, Id);
            BngSettings.ProxyUseMode = (Button_GetCheck(hWndCheck) > 0);            
        }
        break;
        
    default:
        {
            ASSERT(false);
        }
        break;
    };
};


static void BngDlgEditChanged(BngDlg_t* BngDlg, int32 Id)
{
    switch (Id)
    {
    case IDD_BNG_GRP_PARAM_CHANNEL:
        {
            HWND hWndEdit = GetDlgItem(BngDlg->hDlg, IDD_BNG_GRP_PARAM_CHANNEL);
            int32 TargetIdLen = GetWindowTextLengthA(hWndEdit);
            if (TargetIdLen < COUNT_OF(BngSettings.TargetId))
            {
                GetWindowTextA(hWndEdit, &BngSettings.TargetId[0], COUNT_OF(BngSettings.TargetId));
                BngDlgUpdateResult(BngDlg);
            }
            else
            {
                DlgShowErrorMsg(BngDlg->hDlg, IDS_ERR_TARGETID_LARGE, IDS_ERROR);
            };
        }
        break;

    case IDD_BNG_GRP_PARAM_VIEWER:
        {
            HWND hWndEdit = GetDlgItem(BngDlg->hDlg, IDD_BNG_GRP_PARAM_VIEWER);

            char szBuffer[TEXT_BUFF_SIZE];
            szBuffer[0] = '\0';
            GetWindowTextA(hWndEdit, szBuffer, COUNT_OF(szBuffer));

            int32 nViewers = std::atoi(szBuffer);
            nViewers = Clamp(nViewers, 0, BngSettings.ViewersMax);

            BngSettings.Viewers = nViewers;

            SendMessage(BngDlg->hEditVwCtrl, UDM_SETPOS32, 0, LPARAM(nViewers));
        }
        break;

    default:
        ASSERT(false);
        break;
    };
};


static void BngDlgComboboxChanged(BngDlg_t* BngDlg, int32 Id)
{
    HWND hComboBox = GetDlgItem(BngDlg->hDlg, Id);
    ASSERT(hComboBox != NULL);

    int32 Index = ComboBox_GetCurSel(hComboBox);
    if (Index != -1)
    {
        switch (Id)
        {
        case IDD_BNG_GRP_PARAM_PRX:
            {
                NetProxy_t Proxytype = NetProxy_t(ComboBox_GetItemData(hComboBox, Index));
                BngSettings.ProxyType = Proxytype;
            }
            break;

        default:
            {
                ASSERT(false);
            };
            break;
        };
    };
};


static void BngDlgComboboxInit(BngDlg_t* BngDlg, int32 Id)
{
    HWND hComboBox = GetDlgItem(BngDlg->hDlg, Id);
    ASSERT(hComboBox != NULL);

    switch (Id)
    {
    case IDD_BNG_GRP_PARAM_PRX:
        {
            struct ProxyTypeData_t
            {
                const TCHAR* Name;
                NetProxy_t Label;
            };

            static const ProxyTypeData_t ProxyTypeDataArray[] =
            {
                { TEXT("HTTPS"),    NetProxy_Http   },
                { TEXT("SOCKS4"),   NetProxy_Socks4 },
                { TEXT("SOCKS4A"),  NetProxy_Socks4a},
                { TEXT("SOCKS5"),   NetProxy_Socks5 },
            };

            for (int32 i = 0; i < COUNT_OF(ProxyTypeDataArray); ++i)
            {
                ComboBox_InsertString(hComboBox, i, ProxyTypeDataArray[i].Name);
                ComboBox_SetItemData(hComboBox, i, ProxyTypeDataArray[i].Label);
            };

            ASSERT((BngSettings.ProxyType >= 0) && (BngSettings.ProxyType < COUNT_OF(ProxyTypeDataArray)));
            ComboBox_SetCurSel(hComboBox, BngSettings.ProxyType);
        }
        break;
        
    default:
        {
            ASSERT(false);
        }        
        break;
    };
};


static void BngDlgInitializeLocale(BngDlg_t* BngDlg)
{
    DlgLoadCtrlTextId(BngDlg->hDlg, IDD_BNG_GRP_PARAM, IDS_PARAMS);
    DlgLoadCtrlTextId(BngDlg->hDlg, IDD_BNG_GRP_RESULT, IDS_RESULT);
    DlgLoadCtrlTextId(BngDlg->hDlg, IDD_BNG_GRP_RESULT_STRING, IDS_RESULT_FIELD);
    DlgLoadCtrlTextId(BngDlg->hDlg, IDD_BNG_START, IDS_START);
    DlgLoadCtrlTextId(BngDlg->hDlg, IDD_BNG_STOP, IDS_STOP);
    DlgLoadCtrlTextId(BngDlg->hDlg, IDD_BNG_GRP_PARAM_STRPRX, IDS_PRXTYPE);
    DlgLoadCtrlTextId(BngDlg->hDlg, IDD_BNG_GRP_PARAM_STRPRXLIST, IDS_PRXLIST);
    DlgLoadCtrlTextId(BngDlg->hDlg, IDD_BNG_GRP_PARAM_STRPRXMODE, IDS_PROXYMODE);
};


static bool BngDlgOnInitialize(BngDlg_t* BngDlg, HWND hDlg)
{
    BngDlg->hDlg = hDlg;

    EnableThemeDialogTexture(hDlg, ETDT_USETABTEXTURE);
    DlgInitTitleIcon(hDlg, IDS_TITLE, IDI_MODULE);

    BngDlgInitializeLocale(BngDlg);

    BngDlg->hEditVwCtrl = CreateWindowEx(
        WS_EX_LEFT,
        UPDOWN_CLASS,
        NULL,
        WS_CHILDWINDOW | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_HOTTRACK | UDS_NOTHOUSANDS | UDS_WRAP,
        0,
        0,
        0,
        0,
        BngDlg->hDlg,
        NULL,
        BngDlg->hInstance,
        NULL
    );
    ASSERT(BngDlg->hEditVwCtrl != NULL);
    if (BngDlg->hEditVwCtrl)
    {
        HWND hEdit = GetDlgItem(BngDlg->hDlg, IDD_BNG_GRP_PARAM_VIEWER);
        SendMessage(BngDlg->hEditVwCtrl, UDM_SETBUDDY, WPARAM(hEdit), 0);
        SendMessage(BngDlg->hEditVwCtrl, UDM_SETRANGE32, 0, LPARAM(BngSettings.ViewersMax));
    };

    SetWindowTextA(GetDlgItem(BngDlg->hDlg, IDD_BNG_GRP_PARAM_CHANNEL), BngSettings.TargetId);
    SetWindowTextA(GetDlgItem(BngDlg->hDlg, IDD_BNG_GRP_PARAM_VIEWER), std::to_string(BngSettings.Viewers).c_str());

    BngDlgComboboxInit(BngDlg, IDD_BNG_GRP_PARAM_PRX);
    BngDlgComboboxChanged(BngDlg, IDD_BNG_GRP_PARAM_PRX);

    Button_SetCheck(GetDlgItem(BngDlg->hDlg, IDD_BNG_GRP_PARAM_PRXMODE), BngSettings.ProxyUseMode);

    BngDlgUpdateResult(BngDlg);

    return true;
};


static void BngDlgOnTerminate(BngDlg_t* BngDlg)
{
    if (BngDlg->FlagRun)
    {
        DlgDetachTimer(BngDlg->hDlg, IDT_RESULT_EOL_CHK);
        DlgDetachTimer(BngDlg->hDlg, IDT_RESULT_UPDATE);
    };

    if (BngDlg->hEditVwCtrl != NULL)
    {
        DestroyWindow(BngDlg->hEditVwCtrl);
        BngDlg->hEditVwCtrl = NULL;
    };
};


static void BngDlgOnCommand(BngDlg_t* BngDlg, int32 IdCtrl, HWND hWndCtrl, uint32 NotifyCode)
{
    switch (IdCtrl)
    {
    case IDD_BNG_START:
        {
            BngDlgCheckStart(BngDlg);
        }
        break;

    case IDD_BNG_STOP:
        {
            BngDlgCheckStop(BngDlg, true);
        }
        break;

    case IDD_BNG_GRP_PARAM_PRXLIST:
        {
            TCHAR tszPath[MAX_PATH];
            tszPath[0] = TEXT('\0');

            TCHAR* ptszFilter = TEXT("Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0");

            if (DlgGetOpenFilename(BngDlg->hDlg, tszPath, sizeof(tszPath), ptszFilter))
                BngDlgImport(BngDlg, Str_tc2mb(tszPath).c_str());
        }
        break;

    case IDD_BNG_GRP_PARAM_PRXMODE:
        {
            switch (NotifyCode)
            {
            case BN_CLICKED:
                {
                    BngDlgCheckboxChanged(BngDlg, IdCtrl);
                    BngDlgUpdateResult(BngDlg);
                }
                break;
            };
        }
        break;

    case IDD_BNG_GRP_PARAM_PRX:
        {
            switch (NotifyCode)
            {
            case CBN_SELCHANGE:
                BngDlgComboboxChanged(BngDlg, IdCtrl);
                BngDlgUpdateResult(BngDlg);
                break;
            };
        }
        break;

    case IDD_BNG_GRP_PARAM_CHANNEL:
    case IDD_BNG_GRP_PARAM_VIEWER:
        {
            switch (NotifyCode)
            {
            case EN_CHANGE:
                BngDlgEditChanged(BngDlg, IdCtrl);
                BngDlgUpdateResult(BngDlg);
                break;
            };
        }
        break;
    };
};


static void BngDlgOnTimerProc(BngDlg_t* BngDlg, int32 IdTimer)
{
    switch (IdTimer)
    {
    case IDT_RESULT_UPDATE:
        {
            BngDlgUpdateResult(BngDlg);
        }
        break;

    case IDT_RESULT_EOL_CHK:
        {
            if (BngObjIsEol())
                BngDlgCheckStop(BngDlg, false);
        }
        break;

    default:
        ASSERT(false);
        break;
    };
};


static void BngDlgOnLocaleChanged(BngDlg_t* BngDlg)
{
    BngDlgInitializeLocale(BngDlg);
};


static INT_PTR CALLBACK BngDlgMsgProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    BngDlg_t* BngDlg = (BngDlg_t*)GetProp(hDlg, DLG_PROP_INSTANCE);

    switch (Msg)
    {
    case WM_DLGINIT:
        {
            BngDlg = (BngDlg_t*)lParam;
            SetProp(hDlg, DLG_PROP_INSTANCE, BngDlg);
            BngDlgOnInitialize(BngDlg, hDlg);
        }
        return true;

    case WM_DLGTERM:
        {
            BngDlgOnTerminate(BngDlg);
        }
        return true;

    case WM_CLOSE:
        {
            DlgEnd(hDlg);
        }
        return true;

    case WM_COMMAND:
        {
            BngDlgOnCommand(BngDlg, LOWORD(wParam), HWND(lParam), HIWORD(wParam));
        }
        return true;

    case WM_TIMER:
        {
            BngDlgOnTimerProc(BngDlg, int32(wParam));
        }
        return true;

    case WM_DLGLOCALE:
        {
            BngDlgOnLocaleChanged(BngDlg);
        }
        return true;
    };

    return false;
};


HWND BngDlgCreate(HINSTANCE hInstance, HWND hWndParent)
{
    static BngDlg_t BngDlg;
    BngDlg.hInstance = hInstance;
    return DlgBeginModeless(BngDlgMsgProc, hInstance, IDD_BNG, hWndParent, &BngDlg);
};