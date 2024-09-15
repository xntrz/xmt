#pragma once


enum nk_msgbox_type
{
    nk_msgbox_type_info = 0,
    nk_msgbox_type_warn,
    nk_msgbox_type_err,
};


enum nk_msgbox_btn
{
    nk_msgbox_btn_ok = 0,
    nk_msgbox_btn_okcancel,
};


struct nkgdi_window;


DLLSHARED void nk_openfiledlg(
    struct nkgdi_window*            parent,
    std::vector<std::string>&       filepaths,
	const std::vector<std::string>& filter
);


DLLSHARED void nk_savefiledlg(
    struct nkgdi_window*            parent,
    std::string&                    filepath,
    const std::vector<std::string>& filter,
    const std::string&              defaultExt
);


/**
 *  NOTE: title and message should be in UTF8 format
 *  Return values:
 *      >= 0    - value identifies which button pressed, button sequence is identical in a "type" argument
 *      -1      - error call
 */
DLLSHARED int nk_msgbox(
    struct nkgdi_window*    parent,
    nk_msgbox_type          type,
    nk_msgbox_btn           btn,
    const std::string&      title,
    const std::string&      message
);