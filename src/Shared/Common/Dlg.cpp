#include "Dlg.hpp"
#include "Thread.hpp"
#include "Event.hpp"
#include "Configuration.hpp"

#include "Shared/File/File.hpp"
#include "Shared/File/FsRc.hpp"


#define WM_ASYNC_DLG_EXIT (WM_DLGPRIVATE + 1)


enum DlgType_t
{
	DlgType_Modal = 0,
	DlgType_Modeless,
};


struct DlgAsyncParam_t
{
	HWND hDlg;
	HOBJ hEventStart;
};


struct DlgInitParam_t
{
	DlgType_t Type;
	HMODULE hModule;
	void* Param;
	DLGPROC MsgProc;
	int32 Id;
};


struct DlgResizeNode_t : public CListNode<DlgResizeNode_t>
{
	HWND hWnd;
	DlgResizeFlag_t Flags;
};


struct DlgResize_t
{
	HWND Target;
	CList<DlgResizeNode_t> NodeList;
	int32 PrevW;
	int32 PrevH;
	int32 MinW;
	int32 MinH;
};


struct DlgRedirectEntry_t
{
	uint32 Msg;
	WNDPROC Callback;
};


struct DlgRedirect_t
{
	HWND From;
	HWND To;
	WNDPROC OrgMsgProc;
	std::vector<DlgRedirectEntry_t> EntryArray;
};


struct DlgFDrop_t
{
	HDROP hDrop;
	std::vector<std::tstring> PathArray;
};


struct DlgPrivate_t : public CListNode<DlgPrivate_t>
{
	DlgType_t Type;
	DlgResize_t* Resize;
	HMODULE hModule;
	DLGPROC MsgProc;
	int32 Id;
	void* Param;
	HWND Handle;
	std::atomic<int32> RefCount;
	std::atomic<bool>  PendingEndFlag;
	std::atomic<int32> PendingEndResult;
};


struct DlgLocaleInfo_t
{
	DlgLocale_t Locale;
	WORD LangId;
	WORD SublangId;
};


static const DlgLocaleInfo_t DlgLocaleInfoTable[] =
{
	{ DlgLocale_English,	LANG_ENGLISH, SUBLANG_ENGLISH_US	},
	{ DlgLocale_Russian,	LANG_RUSSIAN, SUBLANG_RUSSIAN_RUSSIA},
};


struct DlgContainer_t
{
	DlgLocale_t Locale;
	LANGID LocaleLandId;
	CList<DlgPrivate_t> DlgList;
	std::recursive_mutex Mutex;
	std::atomic<HMODULE> hCurrentModule;
	std::atomic<int32> RefIcon;
	std::atomic<int32> RefMenu;
	std::atomic<int32> RefTimer;
	std::atomic<int32> RefResize;
	std::atomic<int32> RefDlg;
	std::atomic<int32> RefRedirect;
	std::atomic<int32> RefDrop;
	ULONG_PTR GdiPlusToken;
	ITaskbarList3* ITaskBarList;
	bool FlagCoInit;
};


static DlgContainer_t DlgContainer;


static const TCHAR* DLG_PROP_REDIRECT = TEXT("_DlgPropRedirect");


thread_local static HWND DLG_CURRENT = 0;


static void HandleResize(DlgResizeNode_t* ResizeNode, HWND hWndTarget, int32 DeltaX, int32 DeltaY)
{
	UINT SwpFlags = SWP_SHOWWINDOW | SWP_NOZORDER | SWP_NOMOVE;
	RECT RcScreen = { 0 };
	RECT RcClient = { 0 };
	uint32 Flags = ResizeNode->Flags;

	GetWindowRect(ResizeNode->hWnd, &RcScreen);
	GetClientRect(ResizeNode->hWnd, &RcClient);
	MapWindowPoints(ResizeNode->hWnd, hWndTarget, LPPOINT(&RcClient), sizeof(RcClient) / sizeof(POINT));

	if (IS_FLAG_SET_ANY(Flags, DlgResizeFlag_X | DlgResizeFlag_Y))
		FLAG_CLEAR(SwpFlags, SWP_NOMOVE);

	if (IS_FLAG_SET(Flags, DlgResizeFlag_X))
		RcClient.left += DeltaX;

	if (IS_FLAG_SET(Flags, DlgResizeFlag_Y))
		RcClient.top += DeltaY;

	if (IS_FLAG_SET(Flags, DlgResizeFlag_CX))
		RcScreen.right += DeltaX;

	if (IS_FLAG_SET(Flags, DlgResizeFlag_CY))
		RcScreen.bottom += DeltaY;

	SetWindowPos(
		ResizeNode->hWnd,
		NULL,
		RcClient.left,
		RcClient.top,
		RcScreen.right - RcScreen.left,
		RcScreen.bottom - RcScreen.top,
		SwpFlags
	);
};


static BOOL WINAPI DlgMsgRouteProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	DLG_CURRENT = hDlg;
	
	BOOL bResult = FALSE;
	DlgPrivate_t* DlgPrivate = (DlgPrivate_t*)GetWindowLongPtr(hDlg, GWL_USERDATA);

	//
	//	Pre dispatch phase
	//
	switch (uMsg)
	{	
	case WM_INITDIALOG:
		{
			uMsg = WM_DLGINIT;
		}
		break;

	case WM_NCDESTROY:
		{		
			uMsg = WM_DLGTERM;
		}
		break;

	case WM_HOTKEY:
		{
			if (GetActiveWindow() != hDlg)
				return bResult;
		}
		break;

	case WM_GETMINMAXINFO:
		{
			if (DlgPrivate && DlgPrivate->Resize)
			{
				LPMINMAXINFO pMinMaxInfo = LPMINMAXINFO(lParam);
				pMinMaxInfo->ptMinTrackSize.x = DlgPrivate->Resize->MinW;
				pMinMaxInfo->ptMinTrackSize.y = DlgPrivate->Resize->MinH;
			};			
		}
		break;

	case WM_SIZE:
		{
			if (DlgPrivate && DlgPrivate->Resize)
			{
				int32 W = LOWORD(lParam);
				int32 H = HIWORD(lParam);
				int32 DeltaX = W - DlgPrivate->Resize->PrevW;
				int32 DeltaY = H - DlgPrivate->Resize->PrevH;

				for (auto& it : DlgPrivate->Resize->NodeList)
					HandleResize(&it, DlgPrivate->Resize->Target, DeltaX, DeltaY);

				DlgPrivate->Resize->PrevW = W;
				DlgPrivate->Resize->PrevH = H;

				InvalidateRect(hDlg, NULL, FALSE);
			};
		}
		break;

	case WM_DLGBEGIN:
		{
			++DlgPrivate->RefCount;
		}
		break;

	case WM_DLGEND:
		{
			if (!--DlgPrivate->RefCount)
			{
				if (DlgPrivate->PendingEndFlag)
				{
					DlgPrivate->PendingEndFlag = false;
					DlgEnd(hDlg, DlgPrivate->PendingEndResult);
				};
			};
		}
		break;

	case WM_DLGLOCALE:
		{
			SetThreadUILanguage(LANGID(wParam));
		}
		break;
	};

	if (uMsg == WM_DLGINIT)
	{
		++DlgContainer.RefDlg;
		
		DlgInitParam_t* InitParam = (DlgInitParam_t*)lParam;

		ASSERT(!DlgPrivate);
		DlgPrivate = new DlgPrivate_t();
		ASSERT(DlgPrivate);

		{
			std::unique_lock<std::recursive_mutex> Lock(DlgContainer.Mutex);
			ASSERT(!DlgContainer.DlgList.contains(DlgPrivate));
			DlgContainer.DlgList.push_back(DlgPrivate);
		}

		SetWindowLongPtr(hDlg, GWL_USERDATA, LONG(DlgPrivate));

		DlgPrivate->Type = InitParam->Type;
		DlgPrivate->Param = InitParam->Param;
		DlgPrivate->hModule = InitParam->hModule;
		DlgPrivate->Id = InitParam->Id;
		DlgPrivate->MsgProc = InitParam->MsgProc;
		DlgPrivate->Handle = hDlg;

		HWND hWndParent = GetParent(hDlg);
		if (hWndParent)
			PostMessage(hWndParent, WM_DLGBEGIN, InitParam->Id, LPARAM(InitParam->hModule));

		lParam = LPARAM(DlgPrivate->Param);
	};

	//
	//	Dispatch phase
	//
	if (DlgPrivate)
	{
		HMODULE hPrevModule = DlgContainer.hCurrentModule;
		DlgContainer.hCurrentModule = DlgPrivate->hModule;

		bResult = (DlgPrivate->MsgProc(hDlg, uMsg, wParam, lParam) ? TRUE : FALSE);

		DlgContainer.hCurrentModule = hPrevModule;
	};

	//
	//	Post dispatch phase
	//
	if (uMsg == WM_DLGTERM)
	{
		HWND hWndParent = GetParent(hDlg);
		if (hWndParent)
			PostMessage(hWndParent, WM_DLGEND, DlgPrivate->Id, LPARAM(DlgPrivate->hModule));
		
		SetWindowLongPtr(hDlg, GWL_USERDATA, 0);

		{
			std::unique_lock<std::recursive_mutex> Lock(DlgContainer.Mutex);
			ASSERT(DlgContainer.DlgList.contains(DlgPrivate));
			DlgContainer.DlgList.erase(DlgPrivate);
		}
		
		delete DlgPrivate;

		ASSERT(DlgContainer.RefDlg > 0);
		--DlgContainer.RefDlg;
	};

	DLG_CURRENT = 0;
	return bResult;
};


static LRESULT CALLBACK DlgRedirectProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	DlgRedirect_t* DlgRedirect = (DlgRedirect_t*)GetProp(hDlg, DLG_PROP_REDIRECT);
	ASSERT(DlgRedirect);
	ASSERT(DlgRedirect->From == hDlg);

	for (uint32 i = 0; i < DlgRedirect->EntryArray.size(); ++i)
	{
		const DlgRedirectEntry_t& Entry = DlgRedirect->EntryArray[i];

		if (Entry.Msg == uMsg)
			return SendMessage(DlgRedirect->To, uMsg, wParam, lParam);
	};

	return CallWindowProc(DlgRedirect->OrgMsgProc, hDlg, uMsg, wParam, lParam);
};


static int32 DlgShowMessageBox(HWND hWnd, int32 IdText, int32 IdTitle, int32 Type)
{
	TCHAR tszText[TEXT_BUFF_SIZE];
	TCHAR tszTitle[TEXT_BUFF_SIZE];

	tszText[0] = TEXT('\0');
	tszTitle[0] = TEXT('\0');

	DlgLoadText(IdText, tszText, sizeof(tszText));
	DlgLoadText(IdTitle, tszTitle, sizeof(tszTitle));

	static const uint32 TypeFlags[] =
	{
		MB_ICONERROR | MB_OK,
		MB_ICONWARNING | MB_OK,
		MB_ICONASTERISK | MB_OK,
	};

	ASSERT(Type >= 0 && Type < COUNT_OF(TypeFlags));

	return MessageBox(hWnd, tszText, tszTitle, TypeFlags[Type]);
};


bool DlgInitialize(void)
{
	//
	//	Init common controls
	//
	INITCOMMONCONTROLSEX CommonControls = { 0 };
	CommonControls.dwSize = sizeof(INITCOMMONCONTROLSEX);
	CommonControls.dwICC = ICC_LISTVIEW_CLASSES		|
							ICC_TREEVIEW_CLASSES	|
							ICC_BAR_CLASSES			|
							ICC_TAB_CLASSES			|
							ICC_UPDOWN_CLASS		|
							ICC_PROGRESS_CLASS		|
							ICC_HOTKEY_CLASS		|
							ICC_ANIMATE_CLASS		|
							ICC_WIN95_CLASSES		|
							ICC_DATE_CLASSES		|
							ICC_USEREX_CLASSES		|
							ICC_COOL_CLASSES		|
							ICC_INTERNET_CLASSES	|
							ICC_PAGESCROLLER_CLASS	|
							ICC_NATIVEFNTCTL_CLASS	|
							ICC_STANDARD_CLASSES	|
							ICC_LINK_CLASS;
	if (!InitCommonControlsEx(&CommonControls))
	{
		OUTPUTLN("init common ctrl failed %u", GetLastError());
		return false;
	};

	//
	//	Init GDI+ library
	//
	Gdiplus::GdiplusStartupInput GdiPlusInput;
	Gdiplus::Status GdiPlusStatus = Gdiplus::GdiplusStartup(&DlgContainer.GdiPlusToken, &GdiPlusInput, nullptr);
	if (GdiPlusStatus != Gdiplus::Ok)
	{
		OUTPUTLN("init gdi+ failed %u", GetLastError());
		return false;
	};

	//
	//	Init COM library and COM-based components
	//
	HRESULT hResult = CoInitialize(NULL);
	if (FAILED(hResult))
	{
		OUTPUTLN("COM lib init failed hr=0x%X, err=%u", hResult, GetLastError());
		return false;
	};
	
	DlgContainer.FlagCoInit = true;

	hResult = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&DlgContainer.ITaskBarList));
	if (FAILED(hResult))
	{
		OUTPUTLN("win7 task bar init failed hr=0x%x, err=%u", hResult, GetLastError());
		return false;
	};

	//
	//	Init dlg container
	//
	DlgContainer.hCurrentModule = CfgGetAppInstance();
	DlgContainer.RefIcon = 0;
	DlgContainer.RefMenu = 0;
	DlgContainer.RefTimer = 0;
	DlgContainer.RefResize = 0;
	DlgContainer.RefDlg = 0;
	DlgContainer.RefRedirect = 0;
	DlgContainer.RefDrop = 0;
	
	DlgLocaleSet(DlgLocale_Default);
	
	return true;
};


void DlgTerminate(void)
{
	ASSERT(DlgContainer.RefDrop == 0);
	ASSERT(DlgContainer.RefRedirect == 0);
	ASSERT(DlgContainer.RefDlg == 0);
	ASSERT(DlgContainer.RefResize == 0);
	ASSERT(DlgContainer.RefTimer == 0);
	ASSERT(DlgContainer.RefMenu == 0);
	ASSERT(DlgContainer.RefIcon == 0);

	//
	//	Shutdown COM library and COM-based components
	//
	if (DlgContainer.FlagCoInit)
	{
		if (DlgContainer.ITaskBarList)
		{
			DlgContainer.ITaskBarList->Release();
			DlgContainer.ITaskBarList = nullptr;
		};

		CoUninitialize();
		DlgContainer.FlagCoInit = false;
	};

	//
	//	Shudown GDI+ library
	//
	if (DlgContainer.GdiPlusToken)
	{
		Gdiplus::GdiplusShutdown(DlgContainer.GdiPlusToken);
		DlgContainer.GdiPlusToken = NULL;
	};
};


void DlgLocaleSet(DlgLocale_t Locale)
{
	const DlgLocaleInfo_t* DlgLocaleInfo = nullptr;
	
	if (Locale == DlgLocale_SystemDefault)
	{
		LANGID LangIdDefault = GetUserDefaultLangID();

		for (int32 i = 0; i < COUNT_OF(DlgLocaleInfoTable); ++i)
		{
			if (DlgLocaleInfo[i].LangId == PRIMARYLANGID(LangIdDefault) &&
				DlgLocaleInfo[i].SublangId == SUBLANGID(LangIdDefault))
			{
				DlgLocaleInfo = &DlgLocaleInfoTable[i];
				break;
			};
		};

		if (!DlgLocaleInfo)
			DlgLocaleSet(DlgLocale_Default);
	}
	else
	{
		for (int32 i = 0; i < COUNT_OF(DlgLocaleInfoTable); ++i)
		{
			if (DlgLocaleInfoTable[i].Locale == Locale)
			{
				DlgLocaleInfo = &DlgLocaleInfoTable[i];
				break;
			};
		};

		ASSERT(DlgLocaleInfo);
	};

	if (DlgLocaleInfo)
	{
		DlgContainer.Locale = Locale;
		DlgContainer.LocaleLandId = MAKELANGID(DlgLocaleInfo->LangId, DlgLocaleInfo->SublangId);

		{
			std::unique_lock<std::recursive_mutex> Lock(DlgContainer.Mutex);
			for (auto& it : DlgContainer.DlgList)
				PostMessage(it.Handle, WM_DLGLOCALE, WPARAM(DlgContainer.LocaleLandId), 0);
		}
	};
};


HINSTANCE DlgSetCurrentModule(HINSTANCE hInstance)
{
	HINSTANCE hPrevInstance = DlgContainer.hCurrentModule;
	DlgContainer.hCurrentModule = hInstance;
	return hPrevInstance;
};


int32 DlgBeginModal(DLGPROC MsgProc, HMODULE hModule, int32 Id, HWND hWndParent, void* Param)
{
	DlgInitParam_t InitParam = {};
	InitParam.Type 		= DlgType_Modal;
	InitParam.hModule 	= hModule;
	InitParam.Param 	= Param;
	InitParam.MsgProc 	= MsgProc;
	InitParam.Id 		= Id;
	
	return DialogBoxParam(
		hModule,
		MAKEINTRESOURCE(Id),
		hWndParent,
		DlgMsgRouteProc,
		LPARAM(&InitParam)
	);
};


int32 DlgBeginModal(DLGPROC MsgProc, int32 Id, HWND hWndParent, void* Param)
{
	return DlgBeginModal(MsgProc, GetModuleHandle(NULL), Id, hWndParent, Param);
};


HWND DlgBeginModeless(DLGPROC MsgProc, HMODULE hModule, int32 Id, HWND hWndParent, void* Param)
{
	DlgInitParam_t InitParam = {};
	InitParam.Type 		= DlgType_Modeless;
	InitParam.hModule 	= hModule;
	InitParam.Param 	= Param;
	InitParam.MsgProc 	= MsgProc;
	InitParam.Id 		= Id;

	return CreateDialogParam(
		hModule,
		MAKEINTRESOURCE(Id),
		hWndParent,
		DlgMsgRouteProc,
		LPARAM(&InitParam)
	);
};


HWND DlgBeginModeless(DLGPROC MsgProc, int32 Id, HWND hWndParent, void* Param)
{
	return DlgBeginModeless(MsgProc, GetModuleHandle(NULL), Id, hWndParent, Param);
};


int32 DlgRun(HWND hDlg)
{
	DlgPrivate_t* DlgPrivate = (DlgPrivate_t*)GetWindowLongPtr(hDlg, GWL_USERDATA);
	ASSERT(DlgPrivate);
	
	if (!DlgPrivate)
		return -1;

	switch (DlgPrivate->Type)
	{
	case DlgType_Modeless:
		{
			while (WaitMessage())
			{
				MSG Msg = {};
				PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE);

				if (!IsWindow(hDlg) || !IsDialogMessage(hDlg, &Msg))
				{
					TranslateMessage(&Msg);
					DispatchMessage(&Msg);
				};
			};
		}
		break;
	};

	return 0;
};


void DlgEnd(HWND hDlg, int32 Result)
{
	DlgPrivate_t* DlgPrivate = (DlgPrivate_t*)GetWindowLongPtr(hDlg, GWL_USERDATA);
	ASSERT(DlgPrivate);
	
	if (DlgPrivate->RefCount)
	{
		ASSERT(!DlgPrivate->PendingEndFlag);
		DlgPrivate->PendingEndFlag = true;
		DlgPrivate->PendingEndResult = Result;
	}
	else
	{
		switch (DlgPrivate->Type)
		{
		case DlgType_Modal:
			{
				EndDialog(hDlg, Result);
			}
			break;

		case DlgType_Modeless:
			{
				DestroyWindow(hDlg);
			}
			break;
		};
	};	
};



HMODULE DlgGetModule(HWND hWnd)
{
	DlgPrivate_t* DlgPrivate = (DlgPrivate_t*)GetWindowLongPtr(hWnd, GWL_USERDATA);
	ASSERT(DlgPrivate);
	if (DlgPrivate)
		return DlgPrivate->hModule;
	else
		return NULL;
};


void DlgSetHotkey(HWND hDlg, bool SetFlag, int32 VK, int32 Mod, bool FocusFlag)
{
	if (SetFlag)
		RegisterHotKey(hDlg, VK, Mod, VK);
	else
		UnregisterHotKey(hDlg, VK);
};


HMENU DlgLoadMenu(int32 IdMenu)
{
	HMENU hResult = LoadMenu(DlgContainer.hCurrentModule, MAKEINTRESOURCE(IdMenu));
	ASSERT(hResult);
	if (hResult)
		++DlgContainer.RefMenu;	
	return hResult;
};


void DlgFreeMenu(HMENU hMenu)
{
	ASSERT(hMenu);
	if (DestroyMenu(hMenu) > 0)
	{
		ASSERT(DlgContainer.RefMenu > 0);
		--DlgContainer.RefMenu;
	};
};


HICON DlgLoadIcon(int32 IdIcon, int32 cx, int32 cy)
{
	HICON hResult = NULL;

	HOBJ hRcFile = FileOpen(RcFsBuildPathEx(DlgContainer.hCurrentModule, IdIcon, (const char*)RT_RCDATA), "rb");
	if(hRcFile)
	{
		uint32 FSize = uint32(FileSize(hRcFile));
		char* FBuff = new char[FSize];
		ASSERT(FBuff);

		if(FileRead(hRcFile, FBuff, FSize) == FSize)
		{
			IStream* Stream = SHCreateMemStream(PBYTE(FBuff), FSize);
			if (Stream)
			{
				Gdiplus::Bitmap Bitmap(Stream);
				Bitmap.GetHICON(&hResult);

				Stream->Release();
			};
		};

		FileClose(hRcFile);
	};

	if(!hResult)
		hResult = HICON(LoadImage(DlgContainer.hCurrentModule, MAKEINTRESOURCE(IdIcon), IMAGE_ICON, cx, cy, 0));

	if (!hResult)
		hResult = LoadIcon(0, MAKEINTRESOURCE(IdIcon));

	if (hResult)
		++DlgContainer.RefIcon;
	
	return hResult;
};


void DlgFreeIcon(HICON hIcon)
{
	ASSERT(hIcon);

	if (DestroyIcon(hIcon) > 0)
	{
		ASSERT(DlgContainer.RefIcon > 0);
		--DlgContainer.RefIcon;
	};
};


std::tstring DlgLoadText(int32 IdText)
{
    TCHAR tszBuffer[TEXT_BUFF_SIZE];
    tszBuffer[0] = TEXT('\0');
    
    if (!DlgLoadText(IdText, tszBuffer, sizeof(tszBuffer)))
        ASSERT(false, "Failed to load text with ID: %d", IdText);
    
    return std::tstring(tszBuffer);
};


bool DlgLoadText(int32 IdText, TCHAR* Buffer, int32 BufferSize)
{
	return bool(LoadString(DlgContainer.hCurrentModule, IdText, Buffer, BufferSize) > 0);
};


void DlgLoadCtrlTextA(HWND hDlg, int32 IdCtrl, const char* Buffer)
{
	HWND hCtrl = GetDlgItem(hDlg, IdCtrl);
	ASSERT(hCtrl);
	if (hCtrl)
		SetWindowTextA(hCtrl, Buffer);
};


void DlgLoadCtrlTextW(HWND hDlg, int32 IdCtrl, const wchar_t* Buffer)
{
	HWND hCtrl = GetDlgItem(hDlg, IdCtrl);
	ASSERT(hCtrl);
	if (hCtrl)
		SetWindowTextW(hCtrl, Buffer);
};


void DlgLoadCtrlTextId(HWND hDlg, int32 IdCtrl, int32 IdText)
{
	TCHAR tszBuffer[TEXT_BUFF_SIZE];
	tszBuffer[0] = TEXT('\0');

	if (DlgLoadText(IdText, tszBuffer, sizeof(tszBuffer)))
	{
#ifdef _UNICODE
		DlgLoadCtrlTextW(hDlg, IdCtrl, tszBuffer);
#else
		DlgLoadCtrlTextA(hDlg, IdCtrl, tszBuffer);
#endif		
	};
};


int32 DlgShowErrorMsg(HWND hWnd, int32 idText, int32 idTitle)
{
	return DlgShowMessageBox(hWnd, idText, idTitle, 0);
};


int32 DlgShowAlertMsg(HWND hWnd, int32 idText, int32 idTitle)
{
	return DlgShowMessageBox(hWnd, idText, idTitle, 1);
};


int32 DlgShowNotifyMsg(HWND hWnd, int32 idText, int32 idTitle)
{
	return DlgShowMessageBox(hWnd, idText, idTitle, 2);
};


void DlgAttachTimer(HWND hWnd, int32 idTimer, uint32 intervalMS, TIMERPROC pfnCallback)
{
	bool bResult = bool(SetTimer(hWnd, idTimer, intervalMS, pfnCallback) > 0);
	ASSERT(bResult);
	if (bResult)
		++DlgContainer.RefTimer;
};


void DlgDetachTimer(HWND hWnd, int32 idTimer)
{
	bool bResult = bool(KillTimer(hWnd, idTimer) > 0);
	ASSERT(bResult);
	if (bResult)
	{
		ASSERT(DlgContainer.RefTimer > 0);
		--DlgContainer.RefTimer;
	};
};


RECT DlgGetLocalPoint(HWND hWnd)
{
	RECT Rect;
	GetWindowRect(hWnd, &Rect);
	MapWindowPoints(HWND_DESKTOP, GetParent(hWnd), (LPPOINT)&Rect, 2);
	return Rect;
};


bool DlgGetOpenFilename(HWND hWndParent, TCHAR* buff, int32 buffSize, TCHAR* filter)
{
	TCHAR szFileName[MAX_PATH];
	szFileName[0] = TEXT('\0');

	OPENFILENAME ofn;
	memset(&ofn, 0x00, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner	= hWndParent;
	ofn.lpstrFilter = filter;
	ofn.lpstrFile	= szFileName;
	ofn.nMaxFile	= sizeof(szFileName);
	ofn.Flags		= OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

	if (GetOpenFileName(&ofn))
	{
		lstrcpy(buff, szFileName);
		return true;
	};

	return false;
};


bool DlgGetOpenFilenameEx(HWND hWndParent, TCHAR ppBuff[][MAX_PATH], int32* buffc, TCHAR* filter)
{
	TCHAR szFileName[MAX_PATH];
	szFileName[0] = TEXT('\0');

	OPENFILENAME ofn;
	memset(&ofn, 0x00, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner	= hWndParent;
	ofn.lpstrFilter = filter;
	ofn.lpstrFile	= szFileName;
	ofn.nMaxFile	= sizeof(szFileName);
	ofn.Flags 		= OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT | OFN_NOCHANGEDIR;

	if (GetOpenFileName(&ofn))
	{
		TCHAR* ptr = (TCHAR*)szFileName + ofn.nFileOffset;
		bool bEOL = false;
		int32 i = 0;
		while (i < *buffc)
		{
			if (!*ptr)
			{
				if (bEOL)
				{
					break;
				}
				else
				{
					bEOL = true;
					++ptr;
					continue;
				};
			}
			else
			{
				bEOL = false;
			};
		
			int32 len = lstrlen(ptr);
			lstrcpy(ppBuff[i], szFileName);
			lstrcat(ppBuff[i], TEXT("\\"));
			lstrcat(ppBuff[i], ptr);
			ptr += len;
			++i;
		};

		*buffc = i;
		return true;
	};

	return false;
};


bool DlgGetSaveFilename(HWND hWndParent, TCHAR* buff, int32 buffSize, TCHAR* filter, TCHAR* pszDefaultExtension)
{
	TCHAR szFileName[MAX_PATH];
	szFileName[0] = TEXT('\0');

	OPENFILENAME ofn;
	memset(&ofn, 0x00, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner	= hWndParent;
	ofn.lpstrFilter = filter;
	ofn.lpstrFile	= szFileName;
	ofn.nMaxFile	= sizeof(szFileName);
	ofn.Flags 		= OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
	ofn.lpstrDefExt = pszDefaultExtension;

	if (GetSaveFileName(&ofn))
	{
		lstrcpy(buff, szFileName);
		return true;
	};

	return false;
};


void DlgInitTitleIcon(HWND hDlg, int32 IdText, int32 IdIcon)
{
	std::tstring Text = DlgLoadText(IdText);
	if (Text.length() > 0)
		SetWindowText(hDlg, &Text[0]);

	HICON hIcon = DlgLoadIcon(IdIcon, 32, 32);
	if (hIcon)
	{
		HICON hExIcon = CopyIcon(hIcon);
		if (hExIcon)
		{
			SendMessage(hDlg, WM_SETICON, ICON_BIG, LPARAM(hExIcon));
			SendMessage(hDlg, WM_SETICON, ICON_SMALL, LPARAM(hExIcon));
		};
		DlgFreeIcon(hIcon);
	};
};


HOBJ DlgResizeCreate(HWND hWndTarget)
{
	DlgPrivate_t* DlgPrivate = (DlgPrivate_t*)GetWindowLongPtr(hWndTarget, GWL_USERDATA);
	if (DlgPrivate->Resize)
		return 0;

	DlgResize_t* DlgResize = new DlgResize_t();
	if (!DlgResize)
		return 0;
	
	DlgPrivate->Resize = DlgResize;

	RECT RcMinSize = {};
	GetWindowRect(hWndTarget, &RcMinSize);
	DlgResize->MinW = (RcMinSize.right - RcMinSize.left);
	DlgResize->MinH = (RcMinSize.bottom - RcMinSize.top);

	RECT RcPrevSize = {};
	GetClientRect(hWndTarget, &RcPrevSize);
	DlgResize->PrevW = (RcPrevSize.right - RcPrevSize.left);
	DlgResize->PrevH = (RcPrevSize.bottom - RcPrevSize.top);

	DlgResize->Target = hWndTarget;
	
	++DlgContainer.RefResize;	

	return DlgResize;
};


void DlgResizeDestroy(HOBJ hResize)
{
	DlgResize_t* DlgResize = (DlgResize_t*)hResize;

	DlgPrivate_t* DlgPrivate = (DlgPrivate_t*)GetWindowLongPtr(DlgResize->Target, GWL_USERDATA);
	ASSERT(DlgPrivate);
	DlgPrivate->Resize = nullptr;

	ASSERT(DlgContainer.RefResize > 0);
	--DlgContainer.RefResize;

	auto it = DlgResize->NodeList.begin();
	while (it)
	{
		DlgResizeNode_t* Node = &*it;
		it = DlgResize->NodeList.erase(it);

		delete Node;
	};

	delete DlgResize;
};


void DlgResizeRegist(HOBJ hResize, HWND hWnd, DlgResizeFlag_t Flags)
{
	DlgResize_t* DlgResize = (DlgResize_t*)hResize;

	DlgResizeNode_t* ResizeNode = new DlgResizeNode_t();
	if (ResizeNode)
	{
		ResizeNode->hWnd = hWnd;
		ResizeNode->Flags = Flags;

		DlgResize->NodeList.push_back(ResizeNode);
	};
};


void DlgResizeRegistEx(HOBJ hResize, int32 IdCtrl, DlgResizeFlag_t Flags)
{
	DlgResize_t* DlgResize = (DlgResize_t*)hResize;

	DlgResizeRegist(hResize, GetDlgItem(DlgResize->Target, IdCtrl), Flags);
};


void DlgResizeRemove(HOBJ hResize, HWND hWnd)
{
	DlgResize_t* DlgResize = (DlgResize_t*)hResize;

	auto it = DlgResize->NodeList.begin();
	while (it)
	{
		DlgResizeNode_t* Node = &*it;
		if (Node->hWnd == hWnd)
		{
			DlgResize->NodeList.erase(it);
			delete Node;
			break;
		};
	};
};


HOBJ DlgRedirectCreate(HWND hWndFrom, HWND hWndTo)
{
	DlgRedirect_t* Redirect = new DlgRedirect_t();
	if (!Redirect)
		return 0;

	Redirect->From = hWndFrom;
	Redirect->To = hWndTo;
	Redirect->OrgMsgProc = WNDPROC(GetWindowLongPtr(hWndFrom, GWL_WNDPROC));
	Redirect->EntryArray.reserve(32);
	
	SetProp(hWndFrom, DLG_PROP_REDIRECT, Redirect);
	SetWindowLongPtr(hWndFrom, GWL_WNDPROC, LONG(DlgRedirectProc));
	
	++DlgContainer.RefRedirect;

	return Redirect;
};


void DlgRedirectDestroy(HOBJ hRedirect)
{
	ASSERT(DlgContainer.RefRedirect > 0);
	--DlgContainer.RefRedirect;
	
	DlgRedirect_t* Redirect = (DlgRedirect_t*)hRedirect;
	ASSERT(Redirect);

	SetWindowLongPtr(Redirect->From, GWL_WNDPROC, LONG(Redirect->OrgMsgProc));
	RemoveProp(Redirect->From, DLG_PROP_REDIRECT);

	delete Redirect;
};


void DlgRedirectRegist(HOBJ hRedirect, uint32 Msg)
{
	DlgRedirect_t* Redirect = (DlgRedirect_t*)hRedirect;
	ASSERT(Redirect);

	DlgRedirectEntry_t Entry = {};
	Entry.Msg = Msg;
	Entry.Callback = nullptr;

	Redirect->EntryArray.push_back(Entry);
};


HOBJ DlgFDropBegin(HDROP hDrop)
{
	ASSERT(hDrop != NULL);

	TCHAR tszPath[MAX_PATH];
	tszPath[0] = TEXT('\0');

	int32 FileNum = DragQueryFile(hDrop, 0xFFFFFFFF, tszPath, COUNT_OF(tszPath));
	if (FileNum > 0)
	{
		DlgFDrop_t* FDrop = new DlgFDrop_t();
		if (FDrop)
		{
			FDrop->hDrop = hDrop;
			
			for (int32 i = 0; i < FileNum; ++i)
			{
				DragQueryFile(hDrop, i, tszPath, COUNT_OF(tszPath));
				FDrop->PathArray.push_back(tszPath);
			};

			++DlgContainer.RefDrop;

			return FDrop;
		};
	};
	
	DragFinish(hDrop);

	return nullptr;
};


void DlgFDropEnd(HOBJ hFDrop)
{
	DlgFDrop_t* FDrop = (DlgFDrop_t*)hFDrop;

	ASSERT(DlgContainer.RefDrop > 0);
	--DlgContainer.RefDrop;

	ASSERT(FDrop->hDrop != NULL);
	DragFinish(FDrop->hDrop);

	delete FDrop;
};


int32 DlgFDropGetNum(HOBJ hFDrop)
{
	DlgFDrop_t* FDrop = (DlgFDrop_t*)hFDrop;

	return FDrop->PathArray.size();
};


const TCHAR* DlgFDropGetPath(HOBJ hFDrop, int32 Index)
{
	DlgFDrop_t* FDrop = (DlgFDrop_t*)hFDrop;

	ASSERT(Index >= 0 && Index < int32(FDrop->PathArray.size()));
	return &(FDrop->PathArray[Index])[0];
};


bool DlgTaskbarProgressSetState(HWND hWnd, DlgTbProgressState_t State)
{
	static const TBPFLAG TbpStateToFlagTable[] =
	{
		TBPFLAG::TBPF_NOPROGRESS,
		TBPFLAG::TBPF_INDETERMINATE,
		TBPFLAG::TBPF_NORMAL,
		TBPFLAG::TBPF_PAUSED,
		TBPFLAG::TBPF_ERROR,
	};

	ASSERT(State >= 0 && State < COUNT_OF(TbpStateToFlagTable));

	HRESULT hResult = DlgContainer.ITaskBarList->SetProgressState(hWnd, TbpStateToFlagTable[State]);
	return (!FAILED(hResult));
};


bool DlgTaskbarProgressSetValue(HWND hWnd, int32 Current, int32 Max)
{
	HRESULT hResult = DlgContainer.ITaskBarList->SetProgressValue(hWnd, ULONGLONG(Current), ULONGLONG(Max));
	return (!FAILED(hResult));
};