#include "PrxDlg.hpp"
#include "PrxRc.hpp"
#include "PrxObj.hpp"
#include "PrxResult.hpp"
#include "PrxSettings.hpp"

#include "Shared/Network/NetProxy.hpp"

#include "Shared/Common/Time.hpp"
#include "Shared/Common/Dlg.hpp"


static const int32 IDT_RESULT_UPDATE = 100;
static const int32 IDT_RESULT_EOL_CHK = 101;
static const uint32 TIMER_PERIOD_INTERVAL = 50; // ms
static const TCHAR* DLG_PROP_INSTANCE = TEXT("_PrxDlgInst");


struct PrxDlg_t
{
    HWND hDlg;
    uint32 TimestampStart;
    uint32 TimestampCurrent;
    bool FlagRun;
};


static void PrxDlgUpdateResult(PrxDlg_t* PrxDlg)
{
    std::tstring tszStatsuNone = DlgLoadText(IDS_PROXYSTATUS_NONE);
    std::tstring tszStatsuRun = DlgLoadText(IDS_PROXYSTATUS_RUN);

    if (!PrxDlg->FlagRun)
    {
        EnableWindow(
            GetDlgItem(PrxDlg->hDlg, IDD_PRX_START),
            (CPrxResult::Instance().ImportList().count() > 0 ? TRUE : FALSE)
        );
        
        EnableWindow(GetDlgItem(PrxDlg->hDlg, IDD_PRX_STOP), FALSE);
        EnableWindow(GetDlgItem(PrxDlg->hDlg, IDD_PRX_GRP_PARAM_LIST), TRUE);
        
        EnableWindow(
            GetDlgItem(PrxDlg->hDlg, IDD_PRX_GRP_RESULT_EXPORT),
            (CPrxResult::Instance().ExportList().count() > 0 ? TRUE : FALSE)
        );
        
        EnableWindow(GetDlgItem(PrxDlg->hDlg, IDD_PRX_GRP_PARAM_TYPE), TRUE);
        ShowWindow(GetDlgItem(PrxDlg->hDlg, IDD_PRX_PROGRESS), SW_HIDE);
        ShowWindow(GetDlgItem(PrxDlg->hDlg, IDD_PRX_GRP_RESULT_EXPORT), SW_SHOW);

        DlgTaskbarProgressSetState(GetAncestor(PrxDlg->hDlg, GA_ROOT), DlgTbProgressState_NoProgress);
    }
    else
    {
        PrxDlg->TimestampCurrent = TimeCurrentTickPrecise();
        
        ShowWindow(GetDlgItem(PrxDlg->hDlg, IDD_PRX_GRP_RESULT_EXPORT), SW_HIDE);
        ShowWindow(GetDlgItem(PrxDlg->hDlg, IDD_PRX_PROGRESS), SW_SHOW);
        EnableWindow(GetDlgItem(PrxDlg->hDlg, IDD_PRX_START), FALSE);
        EnableWindow(GetDlgItem(PrxDlg->hDlg, IDD_PRX_STOP), TRUE);
        EnableWindow(GetDlgItem(PrxDlg->hDlg, IDD_PRX_GRP_PARAM_LIST), FALSE);
        EnableWindow(GetDlgItem(PrxDlg->hDlg, IDD_PRX_GRP_RESULT_EXPORT), FALSE);
        EnableWindow(GetDlgItem(PrxDlg->hDlg, IDD_PRX_GRP_PARAM_TYPE), FALSE);
        
        SendMessage(
            GetDlgItem(PrxDlg->hDlg, IDD_PRX_PROGRESS),
            PBM_SETRANGE,
            0,
            MAKELPARAM(0, CPrxResult::Instance().ImportList().count())
        );

        SendMessage(
            GetDlgItem(PrxDlg->hDlg, IDD_PRX_PROGRESS),
            PBM_SETPOS,
            CPrxResult::Instance().GetNumProcessed(),
            NULL
        );

        DlgTaskbarProgressSetState(GetAncestor(PrxDlg->hDlg, GA_ROOT), DlgTbProgressState_Normal);
        DlgTaskbarProgressSetValue(GetAncestor(PrxDlg->hDlg, GA_ROOT), CPrxResult::Instance().GetNumProcessed(), CPrxResult::Instance().ImportList().count());
    };

    uint32 Elapsed = (PrxDlg->TimestampCurrent - PrxDlg->TimestampStart);
    uint32 Hours = 0;
    uint32 Minutes = 0;
    uint32 Seconds = 0;
    uint32 Ms = 0;
    TimeStampSlice(Elapsed, &Hours, &Minutes, &Seconds, &Ms);    

    std::tstring tszStatsFormat = DlgLoadText(IDS_PROXY_STATS_TEMPLATE);
    TCHAR Buffer[TEXT_BUFF_SIZE * 4];
    _stprintf(
        Buffer,
        &tszStatsFormat[0],
        (PrxDlg->FlagRun ? &tszStatsuRun[0] : &tszStatsuNone[0]),
        Hours, Minutes, Seconds,
        CPrxResult::Instance().ImportList().count(),
        CPrxResult::Instance().GetNumProcessed(),
        CPrxResult::Instance().GetNumPending(),
        CPrxResult::Instance().GetNumSuccess(),
        CPrxResult::Instance().GetNumFailTotal()
    );
    
    SetWindowText(GetDlgItem(PrxDlg->hDlg, IDD_PRX_GRP_RESULT_VALUE), Buffer);
};


static void PrxDlgCheckStart(PrxDlg_t* PrxDlg)
{
    SendMessage(GetParent(PrxDlg->hDlg), WM_MODULE_REQUEST_LOCK, 0, 0);
    DlgAttachTimer(PrxDlg->hDlg, IDT_RESULT_UPDATE, TIMER_PERIOD_INTERVAL);
    DlgAttachTimer(PrxDlg->hDlg, IDT_RESULT_EOL_CHK, TIMER_PERIOD_INTERVAL);
	
    PrxDlg->FlagRun = true;
    PrxDlg->TimestampStart = TimeCurrentTickPrecise();
    PrxDlgUpdateResult(PrxDlg);
};


static void PrxDlgCheckStop(PrxDlg_t* PrxDlg)
{
	PrxDlg->FlagRun = false;
    PrxDlgUpdateResult(PrxDlg);

    DlgDetachTimer(PrxDlg->hDlg, IDT_RESULT_UPDATE);
    DlgDetachTimer(PrxDlg->hDlg, IDT_RESULT_EOL_CHK);
    SendMessage(GetParent(PrxDlg->hDlg), WM_MODULE_REQUEST_UNLOCK, 0, 0);
};


static void PrxDlgImport(PrxDlg_t* PrxDlg, const char* Path)
{
    CPrxResult::Instance().ImportList().clear();

    if (!CPrxResult::Instance().ImportList().open(Path))
        DlgShowErrorMsg(PrxDlg->hDlg, IDS_ERR_IMPORT_PRX, IDS_ERROR);

    PrxDlgUpdateResult(PrxDlg);
};


static void PrxDlgExport(PrxDlg_t* PrxDlg, const char* Path)
{
    if (CPrxResult::Instance().ExportList().save(Path))
        PrxDlgUpdateResult(PrxDlg);
    else
        DlgShowErrorMsg(PrxDlg->hDlg, IDS_ERR_EXPORT_PRX, IDS_ERROR);
};


static void PrxDlgComboboxChanged(PrxDlg_t* PrxDlg)
{
    HWND hComboBox = GetDlgItem(PrxDlg->hDlg, IDD_PRX_GRP_PARAM_TYPE);
    if (hComboBox != NULL)
    {
        int32 Index = ComboBox_GetCurSel(hComboBox);
        if (Index != -1)
        {
            NetProxy_t Proxytype = NetProxy_t(ComboBox_GetItemData(hComboBox, Index));
            PrxSettings.Type = Proxytype;
        };
    }
    else
    {
        ASSERT(false);
    };
};


static void PrxDlgComboboxInitialize(PrxDlg_t* PrxDlg)
{
    HWND hComboBox = GetDlgItem(PrxDlg->hDlg, IDD_PRX_GRP_PARAM_TYPE);
    if (hComboBox != NULL)
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

        ASSERT((PrxSettings.Type >= 0) && (PrxSettings.Type < COUNT_OF(ProxyTypeDataArray)));
        ComboBox_SetCurSel(hComboBox, PrxSettings.Type);
    }
    else
    {
        ASSERT(false);
    };
};


static void PrxDlgInitializeLocale(PrxDlg_t* PrxDlg)
{
    DlgLoadCtrlTextId(PrxDlg->hDlg, IDD_PRX_GRP_PARAM,          IDS_PROXYPARAMS);
    DlgLoadCtrlTextId(PrxDlg->hDlg, IDD_PRX_GRP_PARAM_STRTYPE,  IDS_PROXYTYPE);
    DlgLoadCtrlTextId(PrxDlg->hDlg, IDD_PRX_GRP_PARAM_STRLIST,  IDS_PROXYLIST);
    DlgLoadCtrlTextId(PrxDlg->hDlg, IDD_PRX_GRP_PARAM_LIST,     IDS_OPEN);
    DlgLoadCtrlTextId(PrxDlg->hDlg, IDD_PRX_GRP_RESULT,         IDS_PROXYRES);
    DlgLoadCtrlTextId(PrxDlg->hDlg, IDD_PRX_GRP_RESULT_EXPORT,  IDS_EXPORT);
    DlgLoadCtrlTextId(PrxDlg->hDlg, IDD_PRX_START,              IDS_START);
    DlgLoadCtrlTextId(PrxDlg->hDlg, IDD_PRX_STOP,               IDS_STOP);
    DlgLoadCtrlTextId(PrxDlg->hDlg, IDD_PRX_GRP_RESULT_STRING,  IDS_PROXYRES_STR);
};


static bool PrxDlgOnInitialize(PrxDlg_t* PrxDlg, HWND hDlg)
{
	PrxDlg->hDlg = hDlg;

    EnableThemeDialogTexture(hDlg, ETDT_USETABTEXTURE);
    DlgInitTitleIcon(hDlg, IDS_TITLE, IDI_MODULE);

    PrxDlgInitializeLocale(PrxDlg);
    PrxDlgUpdateResult(PrxDlg);
    PrxDlgComboboxInitialize(PrxDlg);
    PrxDlgComboboxChanged(PrxDlg);

    ShowWindow(GetDlgItem(PrxDlg->hDlg, IDD_PRX_PROGRESS), SW_HIDE);

    return true;
};


static void PrxDlgOnTerminate(PrxDlg_t* PrxDlg)
{
    if (PrxDlg->FlagRun)
    {
        DlgDetachTimer(PrxDlg->hDlg, IDT_RESULT_EOL_CHK);
        DlgDetachTimer(PrxDlg->hDlg, IDT_RESULT_UPDATE);
    };
};


static void PrxDlgOnCommand(PrxDlg_t* PrxDlg, int32 IdCtrl, HWND hWndCtrl, uint32 NotifyCode)
{
    switch (IdCtrl)
    {
    case IDD_PRX_START:
        {
            PrxDlgCheckStart(PrxDlg);
        }
        break;

    case IDD_PRX_STOP:
        {
            PrxDlgCheckStop(PrxDlg);
        }
        break;

    case IDD_PRX_GRP_PARAM_LIST:
        {            
            TCHAR tszPath[MAX_PATH];
            tszPath[0] = TEXT('\0');

            TCHAR* ptszFilter = TEXT("Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0");
            
            if (DlgGetOpenFilename(PrxDlg->hDlg, tszPath, sizeof(tszPath), ptszFilter))
                PrxDlgImport(PrxDlg, Str_tc2mb(tszPath).c_str());
        }
        break;

    case IDD_PRX_GRP_RESULT_EXPORT:
        {
            TCHAR tszPath[MAX_PATH];
            tszPath[0] = TEXT('\0');

            TCHAR* ptszFilter = TEXT("Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0");
            TCHAR* ptszDefEx = TEXT("txt");
            
            if (DlgGetSaveFilename(PrxDlg->hDlg, tszPath, sizeof(tszPath), ptszFilter, ptszDefEx))
                PrxDlgExport(PrxDlg, Str_tc2mb(tszPath).c_str());
        }
        break;

    case IDD_PRX_GRP_PARAM_TYPE:
        {
            switch (NotifyCode)
            {
            case CBN_SELCHANGE:
                PrxDlgComboboxChanged(PrxDlg);
                break;
            };
        }
        break;
    };
};


static void PrxDlgOnTimerProc(PrxDlg_t* PrxDlg, int32 IdTimer)
{
    switch (IdTimer)
    {
    case IDT_RESULT_UPDATE:
        {
            PrxDlgUpdateResult(PrxDlg);
        }
        break;

    case IDT_RESULT_EOL_CHK:
        {
            if (PrxObjIsEol())
                PrxDlgCheckStop(PrxDlg);
        }
        break;

    default:
        ASSERT(false);
        break;
    };
};


static void PrxDlgOnLocaleChanged(PrxDlg_t* PrxDlg)
{
    PrxDlgInitializeLocale(PrxDlg);
};


static void PrxDlgOnDropFile(PrxDlg_t* PrxDlg, HDROP hDrop)
{
    HOBJ hFDrop = DlgFDropBegin(hDrop);
    if (hFDrop)
    {
        if (DlgFDropGetNum(hFDrop))
        {
            std::string FilePath = Str_tc2mb(DlgFDropGetPath(hFDrop, 0));
            PrxDlgImport(PrxDlg, &FilePath[0]);
        };
        
        DlgFDropEnd(hFDrop);
    };
};


static INT_PTR CALLBACK PrxDlgMsgProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    PrxDlg_t* PrxDlg = (PrxDlg_t*)GetProp(hDlg, DLG_PROP_INSTANCE);
    
    switch (Msg)
    {
    case WM_DLGINIT:
        {
            PrxDlg = (PrxDlg_t*)lParam;
            SetProp(hDlg, DLG_PROP_INSTANCE, PrxDlg);
            PrxDlgOnInitialize(PrxDlg, hDlg);
        }    
        return true;

    case WM_DLGTERM:
        {
            PrxDlgOnTerminate(PrxDlg);
        }
        return true;

    case WM_CLOSE:
        {
            DlgEnd(hDlg);
        }        
        return true;

    case WM_COMMAND:
        {
            PrxDlgOnCommand(PrxDlg, LOWORD(wParam), HWND(lParam), HIWORD(wParam));
        }
        return true;
        
    case WM_TIMER:
        {
            PrxDlgOnTimerProc(PrxDlg, int32(wParam));
        }
        return true;

    case WM_DLGLOCALE:
        {
            PrxDlgOnLocaleChanged(PrxDlg);            
        }
        return true;

    case WM_DROPFILES:
        {
            PrxDlgOnDropFile(PrxDlg, HDROP(wParam));
        }
        return true;
    };

    return false;
};


HWND PrxDlgCreate(HINSTANCE hInstance, HWND hWndParent)
{
    static PrxDlg_t PrxDlg;
    return DlgBeginModeless(PrxDlgMsgProc, hInstance, IDD_PRX, hWndParent, &PrxDlg);
};