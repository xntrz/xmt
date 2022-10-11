#include "TvlDlg.hpp"
#include "TvlRc.hpp"
#include "TvlObj.hpp"
#include "TvlResult.hpp"
#include "TvlSettings.hpp"

#include "Shared/Network/NetProxy.hpp"
#include "Shared/Common/Time.hpp"
#include "Shared/Common/Dlg.hpp"


static const int32 IDT_RESULT_UPDATE = 100;
static const int32 IDT_RESULT_EOL_CHK = 101;
static const uint32 TIMER_PERIOD_INTERVAL = 50; // ms
static const TCHAR* DLG_PROP_INSTANCE = TEXT("_TvlDlgInst");


struct TvlDlg_t
{
    HINSTANCE hInstance;
    HWND hDlg;
    HWND hEditVwCtrl;
    bool FlagRun;
};


static void TvlDlgUpdateResult(TvlDlg_t* TvlDlg)
{
    std::tstring tszStatsuNone = DlgLoadText(IDS_STATUS_NONE);
    std::tstring tszStatsuRun = DlgLoadText(IDS_STATUS_RUN);

    if (!TvlDlg->FlagRun)
    {
        int32 NumTestOkRequire = 1;
        int32 NumTestOk = 0;

        NumTestOk += int32(TvlSettings.TargetId[0] != '\0');
        NumTestOk += int32(CTvlResult::Instance().ImportList().count() > 0);

        EnableWindow(GetDlgItem(TvlDlg->hDlg, IDD_TVL_START), (NumTestOk == NumTestOkRequire) ? TRUE : FALSE);
        EnableWindow(GetDlgItem(TvlDlg->hDlg, IDD_TVL_STOP), FALSE);
        EnableWindow(GetDlgItem(TvlDlg->hDlg, IDD_TVL_GRP_PARAM_CHANNEL), TRUE);
        EnableWindow(GetDlgItem(TvlDlg->hDlg, IDD_TVL_GRP_PARAM_VIEWER), TRUE);
        EnableWindow(GetDlgItem(TvlDlg->hDlg, IDD_TVL_GRP_PARAM_MODE), (TvlSettings.BotType == TvlBotType_Stream) ? TRUE : FALSE);
        EnableWindow(GetDlgItem(TvlDlg->hDlg, IDD_TVL_GRP_PARAM_BOTTYPE), TRUE);
    }
    else
    {
        EnableWindow(GetDlgItem(TvlDlg->hDlg, IDD_TVL_START), FALSE);
        EnableWindow(GetDlgItem(TvlDlg->hDlg, IDD_TVL_STOP), TRUE);
        EnableWindow(GetDlgItem(TvlDlg->hDlg, IDD_TVL_GRP_PARAM_CHANNEL), FALSE);
        EnableWindow(GetDlgItem(TvlDlg->hDlg, IDD_TVL_GRP_PARAM_VIEWER), FALSE);
        EnableWindow(GetDlgItem(TvlDlg->hDlg, IDD_TVL_GRP_PARAM_MODE), FALSE);
        EnableWindow(GetDlgItem(TvlDlg->hDlg, IDD_TVL_GRP_PARAM_BOTTYPE), FALSE);
    };

    std::tstring tszStatsFormat = DlgLoadText(IDS_RESULT_FORMAT);
    TCHAR Buffer[TEXT_BUFF_SIZE * 4];
    _stprintf(
        Buffer,
        &tszStatsFormat[0],
        (TvlDlg->FlagRun ? &tszStatsuRun[0] : &tszStatsuNone[0]),
        CTvlResult::Instance().GetViewerCountWork(),
        CTvlResult::Instance().GetViewerCountReal(),
        CTvlResult::Instance().GetCtxObjWalkTime()
    );

    SetWindowText(GetDlgItem(TvlDlg->hDlg, IDD_TVL_GRP_RESULT_VALUE), Buffer);

    if (TvlSettings.RunMode == TvlRunMode_TimeoutTest)
    {
        EnableWindow(GetDlgItem(TvlDlg->hDlg, IDD_TVL_GRP_PARAM_VIEWER), FALSE);
        EnableWindow(GetDlgItem(TvlDlg->hDlg, IDD_TVL_GRP_PARAM_MODE), FALSE);
        EnableWindow(GetDlgItem(TvlDlg->hDlg, IDD_TVL_GRP_PARAM_BOTTYPE), FALSE);
    }; 
};


static void TvlDlgCheckStart(TvlDlg_t* TvlDlg)
{
    SendMessage(GetParent(TvlDlg->hDlg), WM_MODULE_REQUEST_LOCK, 0, 0);
    DlgAttachTimer(TvlDlg->hDlg, IDT_RESULT_UPDATE, TIMER_PERIOD_INTERVAL);
    DlgAttachTimer(TvlDlg->hDlg, IDT_RESULT_EOL_CHK, TIMER_PERIOD_INTERVAL);

    TvlDlg->FlagRun = true;
    TvlDlgUpdateResult(TvlDlg);

    DlgTaskbarProgressSetState(GetAncestor(TvlDlg->hDlg, GA_ROOT), DlgTbProgressState_Intermediate);
};


static void TvlDlgCheckStop(TvlDlg_t* TvlDlg, bool UserRequestFlag)
{
    DlgTaskbarProgressSetState(GetAncestor(TvlDlg->hDlg, GA_ROOT), DlgTbProgressState_NoProgress);
    
    TvlDlg->FlagRun = false;
    TvlDlgUpdateResult(TvlDlg);

    DlgDetachTimer(TvlDlg->hDlg, IDT_RESULT_UPDATE);
    DlgDetachTimer(TvlDlg->hDlg, IDT_RESULT_EOL_CHK);
    SendMessage(GetParent(TvlDlg->hDlg), WM_MODULE_REQUEST_UNLOCK, 0, 0);

    if (UserRequestFlag)
        return;

    //
    //  Handle error display request
    //
    CTvlResult::ERRTYPE Errtype = CTvlResult::Instance().GetError();
    if (Errtype != CTvlResult::ERRTYPE_NONE)
    {
        //
        //  Exclude NONE error
        //
        int32 Index = (Errtype - 1);

        static const int32 anErrorTextId[] =
        {
            IDS_ERR_CHEXIST,
            IDS_ERR_CHLIVE,
            IDS_ERR_TARGETID_PROTECT,
            IDS_ERR_CLEXIST,
            IDS_ERR_VOEXIST,
        };

        static_assert(COUNT_OF(anErrorTextId) == (CTvlResult::ERRTYPENUM - 1), "update me");
        ASSERT(Index >= 0 && Index < COUNT_OF(anErrorTextId));

        DlgShowErrorMsg(TvlDlg->hDlg, anErrorTextId[Index], IDS_ERROR);

        CTvlResult::Instance().SetError(CTvlResult::ERRTYPE_NONE);
    };
};


static void TvlDlgCheckboxChanged(TvlDlg_t* TvlDlg, int32 Id)
{
    switch (Id)
    {
    case IDD_TVL_GRP_PARAM_MODE:
        {
            HWND hWndCheck = GetDlgItem(TvlDlg->hDlg, Id);
            if (Button_GetCheck(hWndCheck))
            {
                TvlSettings.RunMode = TvlRunMode_Keepalive;
            }
            else
            {
                TvlSettings.RunMode = TvlRunMode_Rst;
            };
        }
        break;
    };
};


static void TvlDlgEditChanged(TvlDlg_t* TvlDlg, int32 Id)
{
    switch (Id)
    {
    case IDD_TVL_GRP_PARAM_CHANNEL:
        {
            HWND hWndEdit = GetDlgItem(TvlDlg->hDlg, IDD_TVL_GRP_PARAM_CHANNEL);
            int32 TargetIdLen = GetWindowTextLengthA(hWndEdit);
            if (TargetIdLen < COUNT_OF(TvlSettings.TargetId))
            {
                GetWindowTextA(hWndEdit, &TvlSettings.TargetId[0], COUNT_OF(TvlSettings.TargetId));
                TvlDlgUpdateResult(TvlDlg);
            }
            else
            {
                DlgShowErrorMsg(TvlDlg->hDlg, IDS_ERR_TARGETID_LARGE, IDS_ERROR);
            };
        }
        break;

    case IDD_TVL_GRP_PARAM_VIEWER:
        {
            HWND hWndEdit = GetDlgItem(TvlDlg->hDlg, IDD_TVL_GRP_PARAM_VIEWER);

            char szBuffer[TEXT_BUFF_SIZE];
            szBuffer[0] = '\0';
            GetWindowTextA(hWndEdit, szBuffer, COUNT_OF(szBuffer));

            int32 nViewers = std::atoi(szBuffer);
            nViewers = Clamp(nViewers, 0, TvlSettings.ViewersMax);

            TvlSettings.Viewers = nViewers;

            SendMessage(TvlDlg->hEditVwCtrl, UDM_SETPOS32, 0, LPARAM(nViewers));
        }
        break;

    default:
        ASSERT(false);
        break;
    };
};


static void TvlDlgComboboxChanged(TvlDlg_t* TvlDlg, int32 Id)
{
    HWND hComboBox = GetDlgItem(TvlDlg->hDlg, Id);
    ASSERT(hComboBox != NULL);

    int32 Index = ComboBox_GetCurSel(hComboBox);
    if (Index != -1)
    {
        switch (Id)
        {
        case IDD_TVL_GRP_PARAM_BOTTYPE:
            {
                TvlSettings.BotType = TvlBotType_t(ComboBox_GetItemData(hComboBox, Index));
                
                static const int32 TargetStrId[] = { IDS_TARGET_CH, IDS_TARGET_CL, IDS_TARGET_VO };
                ASSERT(TvlSettings.BotType >= 0 && TvlSettings.BotType < COUNT_OF(TargetStrId));
                DlgLoadCtrlTextId(TvlDlg->hDlg, IDD_TVL_GRP_PARAM_STRCHANNEL, TargetStrId[TvlSettings.BotType]);
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


static void TvlDlgComboboxInit(TvlDlg_t* TvlDlg, int32 Id)
{
    HWND hComboBox = GetDlgItem(TvlDlg->hDlg, Id);
    ASSERT(hComboBox != NULL);

    switch (Id)
    {
    case IDD_TVL_GRP_PARAM_BOTTYPE:
        {
            struct BotTypeData_t
            {
                const TCHAR* Name;
                TvlBotType_t Type;
            };

            static const BotTypeData_t BotTypeDataArray[] =
            {
                { TEXT("Stream"),   TvlBotType_Stream   },
                { TEXT("Clip"),     TvlBotType_Clip     },
                { TEXT("Vod"),      TvlBotType_Vod      },
            };

            for (int32 i = 0; i < COUNT_OF(BotTypeDataArray); ++i)
            {
                ComboBox_InsertString(hComboBox, i, BotTypeDataArray[i].Name);
                ComboBox_SetItemData(hComboBox, i, BotTypeDataArray[i].Type);
            };

            ASSERT((TvlSettings.BotType >= 0) && (TvlSettings.BotType < COUNT_OF(BotTypeDataArray)));
            ComboBox_SetCurSel(hComboBox, TvlSettings.BotType);
        }
        break;

    default:
        {
            ASSERT(false);
        }
        break;
    };
};


static void TvlDlgInitializeLocale(TvlDlg_t* TvlDlg)
{
    DlgLoadCtrlTextId(TvlDlg->hDlg, IDD_TVL_GRP_PARAM, IDS_PARAMS);
    DlgLoadCtrlTextId(TvlDlg->hDlg, IDD_TVL_GRP_RESULT, IDS_RESULT);
    DlgLoadCtrlTextId(TvlDlg->hDlg, IDD_TVL_GRP_RESULT_STRING, IDS_RESULT_FIELD);
    DlgLoadCtrlTextId(TvlDlg->hDlg, IDD_TVL_START, IDS_START);
    DlgLoadCtrlTextId(TvlDlg->hDlg, IDD_TVL_STOP, IDS_STOP);
    DlgLoadCtrlTextId(TvlDlg->hDlg, IDD_TVL_GRP_PARAM_STRMODE, IDS_KEEPALIVEMODE);
    DlgLoadCtrlTextId(TvlDlg->hDlg, IDD_TVL_GRP_PARAM_STRBOTTYPE, IDS_BOTTYPE);
    DlgLoadCtrlTextId(TvlDlg->hDlg, IDD_TVL_GRP_PARAM_STRCHANNEL, IDS_TARGET_CH);
};


static bool TvlDlgOnInitialize(TvlDlg_t* TvlDlg, HWND hDlg)
{
    TvlDlg->hDlg = hDlg;

    EnableThemeDialogTexture(hDlg, ETDT_USETABTEXTURE);
    DlgInitTitleIcon(hDlg, IDS_TITLE, IDI_MODULE);

    TvlDlgInitializeLocale(TvlDlg);
    TvlDlgUpdateResult(TvlDlg);

    TvlDlg->hEditVwCtrl = CreateWindowEx(
        WS_EX_LEFT,
        UPDOWN_CLASS,
        NULL,
        WS_CHILDWINDOW | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_HOTTRACK | UDS_NOTHOUSANDS | UDS_WRAP,
        0,
        0,
        0,
        0,
        TvlDlg->hDlg,
        NULL,
        TvlDlg->hInstance,
        NULL
    );
    ASSERT(TvlDlg->hEditVwCtrl != NULL);
    if (TvlDlg->hEditVwCtrl)
    {
        HWND hEdit = GetDlgItem(TvlDlg->hDlg, IDD_TVL_GRP_PARAM_VIEWER);
        SendMessage(TvlDlg->hEditVwCtrl, UDM_SETBUDDY, WPARAM(hEdit), 0);
        SendMessage(TvlDlg->hEditVwCtrl, UDM_SETRANGE32, 0, LPARAM(TvlSettings.ViewersMax));
    };

    SetWindowTextA(GetDlgItem(TvlDlg->hDlg, IDD_TVL_GRP_PARAM_CHANNEL), TvlSettings.TargetId);
    SetWindowTextA(GetDlgItem(TvlDlg->hDlg, IDD_TVL_GRP_PARAM_VIEWER), std::to_string(TvlSettings.Viewers).c_str());
    Button_SetCheck(GetDlgItem(TvlDlg->hDlg, IDD_TVL_GRP_PARAM_MODE), TvlSettings.RunMode == TvlRunMode_Keepalive ? TRUE : FALSE);

    TvlDlgComboboxInit(TvlDlg, IDD_TVL_GRP_PARAM_BOTTYPE);
    TvlDlgComboboxChanged(TvlDlg, IDD_TVL_GRP_PARAM_BOTTYPE);

    return true;
};


static void TvlDlgOnTerminate(TvlDlg_t* TvlDlg)
{
    if (TvlDlg->FlagRun)
    {
        DlgDetachTimer(TvlDlg->hDlg, IDT_RESULT_EOL_CHK);
        DlgDetachTimer(TvlDlg->hDlg, IDT_RESULT_UPDATE);
    };

    if (TvlDlg->hEditVwCtrl != NULL)
    {
        DestroyWindow(TvlDlg->hEditVwCtrl);
        TvlDlg->hEditVwCtrl = NULL;
    };
};


static void TvlDlgOnCommand(TvlDlg_t* TvlDlg, int32 IdCtrl, HWND hWndCtrl, uint32 NotifyCode)
{
    switch (IdCtrl)
    {
    case IDD_TVL_START:
        {
            TvlDlgCheckStart(TvlDlg);
        }
        break;

    case IDD_TVL_STOP:
        {
            TvlDlgCheckStop(TvlDlg, true);
        }
        break;

    case IDD_TVL_GRP_PARAM_BOTTYPE:
        {
            switch (NotifyCode)
            {
            case CBN_SELCHANGE:
                TvlDlgComboboxChanged(TvlDlg, IdCtrl);
                TvlDlgUpdateResult(TvlDlg);
                break;
            };
        }
        break;

    case IDD_TVL_GRP_PARAM_CHANNEL:
    case IDD_TVL_GRP_PARAM_VIEWER:
        {
            switch (NotifyCode)
            {
            case EN_CHANGE:
                {
                    TvlDlgEditChanged(TvlDlg, IdCtrl);
                    TvlDlgUpdateResult(TvlDlg);
                }
                break;
            };
        }
        break;

    case IDD_TVL_GRP_PARAM_MODE:
        {
            switch (NotifyCode)
            {
            case BN_CLICKED:
                {
                    TvlDlgCheckboxChanged(TvlDlg, IdCtrl);
                    TvlDlgUpdateResult(TvlDlg);
                }
                break;
            };
        }
        break;
    };
};


static void TvlDlgOnTimerProc(TvlDlg_t* TvlDlg, int32 IdTimer)
{
    switch (IdTimer)
    {
    case IDT_RESULT_UPDATE:
        {
            TvlDlgUpdateResult(TvlDlg);
        }
        break;

    case IDT_RESULT_EOL_CHK:
        {
            if (TvlObjIsEol())
                TvlDlgCheckStop(TvlDlg, false);
        }
        break;

    default:
        ASSERT(false);
        break;
    };
};


static void TvlDlgOnLocaleChanged(TvlDlg_t* TvlDlg)
{
    TvlDlgInitializeLocale(TvlDlg);
};


static INT_PTR CALLBACK TvlDlgMsgProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    TvlDlg_t* TvlDlg = (TvlDlg_t*)GetProp(hDlg, DLG_PROP_INSTANCE);

    switch (Msg)
    {
    case WM_DLGINIT:
        {
            TvlDlg = (TvlDlg_t*)lParam;
            SetProp(hDlg, DLG_PROP_INSTANCE, TvlDlg);
            TvlDlgOnInitialize(TvlDlg, hDlg);
        }
        return true;

    case WM_DLGTERM:
        {
            TvlDlgOnTerminate(TvlDlg);
        }
        return true;

    case WM_CLOSE:
        {
            DlgEnd(hDlg);
        }
        return true;

    case WM_COMMAND:
        {
            TvlDlgOnCommand(TvlDlg, LOWORD(wParam), HWND(lParam), HIWORD(wParam));
        }
        return true;

    case WM_TIMER:
        {
            TvlDlgOnTimerProc(TvlDlg, int32(wParam));
        }
        return true;

    case WM_DLGLOCALE:
        {
            TvlDlgOnLocaleChanged(TvlDlg);
        }
        return true;
    };

    return false;
};


HWND TvlDlgCreate(HINSTANCE hInstance, HWND hWndParent)
{
    static TvlDlg_t TvlDlg;
    TvlDlg.hInstance = hInstance;
    return DlgBeginModeless(TvlDlgMsgProc, hInstance, IDD_TVL, hWndParent, &TvlDlg);
};