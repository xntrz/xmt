#include "AppDlgDbg.hpp"
#include "AppRc.hpp"
#include "AppMem.hpp"

#include "Shared/Network/TcpStats.hpp"
#include "Shared/Network/TcpIoSvc.hpp"
#include "Shared/Common/Dlg.hpp"


#define IDT_PERIOD (1000ul / 60ul)


struct AppDlgDbg_t
{
    HWND hDlg;
    HWND hDlgParent;
    bool FlagPause;
};


struct PrintBuff_t
{
	int32 WritePos;
	char Mem[2048];
};


static AppDlgDbg_t AppDlgDbg;


static void PrintBuffered(PrintBuff_t* Buff, const char* Format, ...)
{
	if (Buff->WritePos == 0)
		Buff->Mem[0] = '\0';

	va_list vl;
	va_start(vl, Format);;
	Buff->WritePos += std::vsprintf(&Buff->Mem[Buff->WritePos], Format, vl);
    va_end(vl);

    ASSERT(Buff->WritePos <= COUNT_OF(Buff->Mem));
};


static const char* FormattedSize(uint64 Size)
{    
    //
	//	Abit modified code https://stackoverflow.com/questions/3898840/converting-a-number-of-bytes-into-a-file-size-in-c
	//

    static const char* PrefixArray[] = { "EB", "PB", "TB", "GB", "MB", "KB", "B" };
	static const uint64 Exbibytes = 1024ull * 1024ull * 1024ull * 1024ull * 1024ull * 1024ull;
	thread_local static char FormatBuff[2048];
	FormatBuff[0] = '0';
	FormatBuff[1] = '\0';

	uint64 Mod = Exbibytes;

	for (int32 i = 0; i < COUNT_OF(PrefixArray); ++i, Mod >>= 10)
	{
		if (Size > Mod)
		{            
			if (!(Size & (Mod - 1)))
				std::sprintf(FormatBuff, "%llu %s", (Size / Mod), PrefixArray[i]);
			else
				std::sprintf(FormatBuff, "%.1f %s", (float(Size) / float(Mod)), PrefixArray[i]);

			break;
		};
	};

	return FormatBuff;
};


static INT_PTR CALLBACK AppDlgDbgMsgProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    switch (Msg)
    {
    case WM_DLGINIT:
        {
            AppDlgDbg.hDlg = hDlg;
			AppDlgDbg.FlagPause = true;
            
            RECT RcMe = { 0 };
            GetWindowRect(hDlg, &RcMe);

            RECT RcParent = { 0 };
            GetWindowRect(AppDlgDbg.hDlgParent, &RcParent);

            MoveWindow(
				AppDlgDbg.hDlg,
				RcParent.left - (RcMe.right - RcMe.left),
				RcParent.top,
				(RcMe.right - RcMe.left),
				(RcMe.bottom - RcMe.top),
				TRUE
            );

            PostMessage(AppDlgDbg.hDlg, WM_COMMAND, MAKEWPARAM(IDD_DBG_DISPCTRL, 0), 0);
        }
        return true;

    case WM_DLGTERM:
        {
            if (!AppDlgDbg.FlagPause)
                DlgDetachTimer(AppDlgDbg.hDlg, IDT_PERIOD);
        }
        return true;
        
    case WM_CLOSE:
        {            
            DlgEnd(hDlg);
        }
        return true;

    case WM_TIMER:
        {
            if (wParam == IDT_PERIOD)
            {
                HWND hDisp = GetDlgItem(AppDlgDbg.hDlg, IDD_DBG_DISPLAY);
				PrintBuff_t Buff = {};

                {
                    //
                    //  Print mem diag
                    //
                    int64 AllocNum = 0;
                    int64 CrossThreadAlloc = 0;
                    int64 SelfThreadAlloc = 0;
                    uint32 AllocatedBytes = 0;
                    uint32 LargestAllocSize = 0;
                    uint32 LargestAlloctedSize = 0;

                    AppMemGrabDiag(
                        &AllocNum,
                        &CrossThreadAlloc,
                        &SelfThreadAlloc,
                        &AllocatedBytes,
                        &LargestAllocSize,
                        &LargestAlloctedSize
                    );

					PrintBuffered(&Buff, "Mem diag:\r\n");
					PrintBuffered(&Buff, "%-25s%llu\r\n", "Alloc num:", AllocNum);
					PrintBuffered(&Buff, "%-25s%llu\r\n", "Cross th alloc:", CrossThreadAlloc);
					PrintBuffered(&Buff, "%-25s%llu\r\n", "Self  th alloc:", SelfThreadAlloc);
					PrintBuffered(&Buff, "%-25s%s\r\n", "Allocated bytes:", FormattedSize(AllocatedBytes));
					PrintBuffered(&Buff, "%-25s%s\r\n", "Largest alloc size:", FormattedSize(LargestAllocSize));
					PrintBuffered(&Buff, "%-25s%s\r\n", "Larget allocated size:", FormattedSize(LargestAlloctedSize));
					PrintBuffered(&Buff, "\r\n");
                }

                {
                    //
                    //  Print net tcp/udp diag
                    //
                    uint64 TcpLastSecBytesRecvAccum = 0;
                    uint64 TcpLastSecBytesSentAccum = 0;
                    uint64 TcpLastSecBytesRecv = 0;
                    uint64 TcpLastSecBytesSent = 0;
                    uint64 TcpTotalBytesRecv = 0;
                    uint64 TcpTotalBytesSent = 0;
                    
                    TcpStatsGrab(
                        &TcpLastSecBytesRecvAccum,
                        &TcpLastSecBytesSentAccum,
                        &TcpLastSecBytesRecv,
                        &TcpLastSecBytesSent,
                        &TcpTotalBytesRecv,
                        &TcpTotalBytesSent                    
                    );

                    int32 TcpIoNum = 0;
                    int32 TcpIoThreadsNum = 0;
                    int32 TcpIoThreadsAvail = 0;
                    TcpIoSvcStats(&TcpIoNum, &TcpIoThreadsNum, &TcpIoThreadsAvail);

                    PrintBuffered(&Buff, "Tcp diag:\r\n");
                    PrintBuffered(&Buff, "%-15s%s\r\n", "Recv total:", FormattedSize(TcpTotalBytesRecv));
					PrintBuffered(&Buff, "%-15s%s\r\n", "Send total:", FormattedSize(TcpTotalBytesSent));
                    PrintBuffered(&Buff, "%-15s%s\r\n", "Recv bytes/s:", FormattedSize(TcpLastSecBytesRecv));
                    PrintBuffered(&Buff, "%-15s%s\r\n", "Send bytes/s:", FormattedSize(TcpLastSecBytesSent));
                    PrintBuffered(&Buff, "%-15s%d\r\n", "Io num:", TcpIoNum);
                    PrintBuffered(&Buff, "%-15s%d\r\n", "Io th num:", TcpIoThreadsNum);
                    PrintBuffered(&Buff, "%-15s%d\r\n", "Io th avail:", TcpIoThreadsAvail);
                }

                if (Buff.WritePos > 0)
                    SetWindowTextA(hDisp, Buff.Mem);
            };
        }
        return true;

    case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
            case IDD_DBG_DISPCTRL:
                {
                    if (AppDlgDbg.FlagPause)
                    {
                        AppDlgDbg.FlagPause = false;                        
                        DlgLoadCtrlTextId(AppDlgDbg.hDlg, IDD_DBG_DISPCTRL, IDS_DBG_PAUSE);
                        DlgAttachTimer(AppDlgDbg.hDlg, IDT_PERIOD, 500);
                    }
                    else
                    {
                        AppDlgDbg.FlagPause = true;
                        DlgLoadCtrlTextId(AppDlgDbg.hDlg, IDD_DBG_DISPCTRL, IDS_DBG_RESUME);
                        DlgDetachTimer(AppDlgDbg.hDlg, IDT_PERIOD);
                    };
                }
                break;
            };
        }
        return true;

    case WM_MOVING:
        {
            //
            //  Disable moving dbg window by mouse and move only 
            //  if parent window is moving via AppDlgDbgOnMoving.
            //            
            RECT* RcPositionNew = LPRECT(lParam);
            RECT RcPositionCur = { 0 };
            GetWindowRect(hDlg, &RcPositionCur);

            *RcPositionNew = RcPositionCur;
        }
        return true;
    };

    return false;
};


HWND AppDlgDbgCreate(HWND hWndParent)
{
    AppDlgDbg.hDlgParent = hWndParent;
    return DlgBeginModeless(AppDlgDbgMsgProc, IDD_DBG, hWndParent);
};


void AppDlgDbgOnMoving(const RECT* RcDelta)
{
    if (AppDlgDbg.hDlg == NULL)
        return;
    
    RECT RcPositionNow = { 0 };
    GetWindowRect(AppDlgDbg.hDlg, &RcPositionNow);

    RcPositionNow.left  += RcDelta->left;
    RcPositionNow.right += RcDelta->right;
    RcPositionNow.top   += RcDelta->top;
    RcPositionNow.bottom+= RcDelta->bottom;

    MoveWindow(
        AppDlgDbg.hDlg,
        RcPositionNow.left,
        RcPositionNow.top,
        (RcPositionNow.right - RcPositionNow.left),
        (RcPositionNow.bottom - RcPositionNow.top),
        TRUE
    );
};