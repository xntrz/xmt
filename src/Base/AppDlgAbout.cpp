#include "AppDlgAbout.hpp"
#include "AppRc.hpp"
#include "AppVersion.hpp"

#include "Shared/Common/Dlg.hpp"


static INT_PTR CALLBACK AppDlgAboutMsgProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    switch (Msg)
    {
    case WM_DLGINIT:
        {
            DlgInitTitleIcon(hDlg, IDS_ABOUT, IDI_APPICO);
            
            HICON hIcon = DlgLoadIcon(IDI_APPICO, 32, 32);
            ASSERT(hIcon);
            if (hIcon)
            {
                Static_SetIcon(GetDlgItem(hDlg, IDD_ABOUT_LOGO), hIcon);
                DlgFreeIcon(hIcon);
            };

            std::tstring NameBuffer;
            std::tstring YearBuffer;

            std::string buff;
            buff.reserve(256);
            
            buff += APP_NAME;
            buff += " ";
            buff += APP_VER_FILE;
            DlgLoadCtrlText(hDlg, IDD_ABOUT_TITLE, std::tstring(buff.begin(), buff.end()).c_str());

            buff.clear();
            buff += APP_COPYR;
            DlgLoadCtrlText(hDlg, IDD_ABOUT_COPYR, std::tstring(buff.begin(), buff.end()).c_str());
        }
        return true;

    case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
            case IDD_ABOUT_OK:
                {
                    PostMessage(hDlg, WM_CLOSE, 0, 0);
                }
                break;
            };
        }
        return true;

    case WM_CLOSE:
        {
            DlgEnd(hDlg);
        }
        return true;
    };

    return false;
};


void AppDlgAboutCreate(HWND hWndParent)
{
    DlgBeginModal(AppDlgAboutMsgProc, IDD_ABOUT, hWndParent);
};