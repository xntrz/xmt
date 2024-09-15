#include "nk_commdlg.hpp"
#include "nk_text.hpp"
#include "nk_font.hpp"
#include "nk_window.hpp"
#include "nk_utils.hpp"


/**
 *  Now use native comm dlg
 *  TODO: open/save file dialog impl
 */
#define NK_COMMDLG_USE_NATIVE


struct nk_commdlg_ctx
{
    enum type
    {
        type_open = 0,
        type_save,
        type_msgbox,
    };
    
    enum state
    {
        state_init = 0,
        state_run,
    };
    
    type type;
    state state;
    std::vector<std::string> filepaths;
    const std::vector<std::string> filter;
    std::string defaultExt;
    nk_msgbox_type msgboxType;
    nk_msgbox_btn msgboxBtn;
    std::string msgboxTitle;
    std::string msgboxMsg;

    /* open file dlg c-tor */
    inline nk_commdlg_ctx(const std::vector<std::string>& _filter)
        : type(type_open), state(state_init), filter(_filter) {};

    /* save file dlg c-tor */
    inline nk_commdlg_ctx(const std::vector<std::string>& _filter, const std::string& _defaultExt)
        : type(type_save), state(state_init), filter(_filter), defaultExt(_defaultExt) {};

    /* msgbox dlg c-tor */
    inline nk_commdlg_ctx(nk_msgbox_type _msgboxType, nk_msgbox_btn _btn, const std::string& _title, const std::string& _msg)
        : type(type_msgbox), state(state_init), msgboxType(_msgboxType), msgboxBtn(_btn), msgboxTitle(_title), msgboxMsg(_msg) {};
};


static const char nk_commdlg_locale_en[] =
{
#include "nk_commdlg.text_en" 
};


static void nk_commdlg_msgbox_draw_proc(
    struct nkgdi_window*    wnd,
    struct nk_context*      ctx
)
{
    ;
};


static void nk_commdlg_fileopen_draw_proc(
    struct nkgdi_window*    wnd,
    struct nk_context*      ctx
)
{
    ;
};


static void nk_commdlg_filesave_draw_proc(
    struct nkgdi_window*    wnd,
    struct nk_context*      ctx
)
{
    ;
};


static bool nk_commdlg_draw_proc(
    struct nkgdi_window*    wnd,
    struct nk_context*      ctx,
    bool                    /*ret*/
)
{
    nkgdi_window_set_min_size(wnd, 600, 300);

    nk_commdlg_ctx* dlgctx;
    nkgdi_window_get_param(wnd, reinterpret_cast<void**>(&dlgctx));

    switch (dlgctx->state)
    {
    case nk_commdlg_ctx::state_init:
        {
            nk_text_load("nk_commdlg_en", nk_commdlg_locale_en, sizeof(nk_commdlg_locale_en));
            dlgctx->state = nk_commdlg_ctx::state_run;
            nkgdi_window_post_redraw(wnd);
        }
        break;

    case nk_commdlg_ctx::state_run:
        {
            nk_text_path_locale_push("nk_commdlg");
            
            switch (dlgctx->type)
            {
            case nk_commdlg_ctx::type_save:
                nk_commdlg_filesave_draw_proc(wnd, ctx);
                break;
                
            case nk_commdlg_ctx::type_open:
                nk_commdlg_fileopen_draw_proc(wnd, ctx);
                break;
                
            case nk_commdlg_ctx::type_msgbox:
                nk_commdlg_msgbox_draw_proc(wnd, ctx);
                break;

            default:
                ASSERT(false, "unknown dlg type (%d)", dlgctx->type);
                break;
            };
            
            nk_text_path_pop();
        }
        break;
    };

    return false;
};


static inline std::vector<char> nk_commdlg_make_filter(const std::vector<std::string>& filter)
{
	std::vector<char> filterbuff;
	filterbuff.reserve(MAX_PATH * 2);

	std::for_each(filter.begin(), filter.end(), [&filterbuff](const std::string& ext) {
		filterbuff.insert(filterbuff.end(), ext.begin(), ext.end());
		filterbuff.insert(filterbuff.end(), '\0');
		if (ext[0] != '*')
			filterbuff.insert(filterbuff.end(), '*');
		filterbuff.insert(filterbuff.end(), ext.begin(), ext.end());
		filterbuff.insert(filterbuff.end(), '\0');
	});

	return filterbuff;
};


/*DLLSHARED*/ void nk_openfiledlg(
    struct nkgdi_window*            parent,
    std::vector<std::string>&       filepaths,
    const std::vector<std::string>& filter
)
{
#ifdef NK_COMMDLG_USE_NATIVE    
    std::string fbuff;
    fbuff.resize(MAX_PATH * 256);

	std::vector<char> filterbuff = nk_commdlg_make_filter(filter);

    ::OPENFILENAMEA ofn;
    std::memset(&ofn, 0x00, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = nkgdi_window_get_native(parent);
    ofn.lpstrFilter = filterbuff.data();
	ofn.nFilterIndex= 1;
    ofn.lpstrFile   = &fbuff[0];
    ofn.nMaxFile    = fbuff.length();
    ofn.Flags       = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT | OFN_NOCHANGEDIR;

    if (::GetOpenFileNameA(&ofn))
    {
		std::size_t offset = ofn.nFileOffset;
		std::string filename;
		do
		{
			filename = std::string(&fbuff[0] + offset);
			offset += (filename.length() + 1);
			if (!filename.empty())
			{
				std::string filepath;
                filepath += fbuff.substr(0u, ofn.nFileOffset - 1);
                if(filepath.back() != '\\')
                    filepath += '\\';
				filepath += filename;

				filepaths.push_back(std::move(filepath));
			};
		} while (!filename.empty());
    };
#else
    nk_commdlg_ctx ctx(filter);
        
    nkgdi_window_show(
        "",
        760,
        480,
        -1,
        -1,
        nk_font_get_name(),
        nk_font_get_height(),
        nkgdi_window_flag_close | nkgdi_window_flag_resize,
        nk_commdlg_draw_proc,
        parent,
        &ctx
    );

    filepaths = ctx.filepaths;
#endif    
};


/*DLLSHARED*/ void nk_savefiledlg(
    struct nkgdi_window*            parent,
    std::string&                    filepath,
    const std::vector<std::string>& filter,
    const std::string&              defaultExt
)
{
#ifdef NK_COMMDLG_USE_NATIVE    
    std::string fbuff;
    fbuff.resize(MAX_PATH * 4);

	std::vector<char> filterbuff = nk_commdlg_make_filter(filter);

    ::OPENFILENAMEA ofn;
    std::memset(&ofn, 0x00, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = nkgdi_window_get_native(parent);
    ofn.lpstrFilter = filterbuff.data();
    ofn.nFilterIndex= 1;
    ofn.lpstrFile   = &fbuff[0];
    ofn.nMaxFile    = fbuff.length();
    ofn.Flags       = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = defaultExt.c_str();

    if (::GetSaveFileNameA(&ofn))
        filepath = fbuff;
#else
    nk_commdlg_ctx ctx(filter, defaultExt);
    
    nkgdi_window_show(
        "",
        760,
        480,
        -1,
        -1,
        nk_font_get_name(),
        nk_font_get_height(),
        nkgdi_window_flag_close | nkgdi_window_flag_resize,
        nk_commdlg_draw_proc,
        parent,
        &ctx
    );
    
    if (!ctx.filepaths.empty())
        filepath = ctx.filepaths[0];
#endif    
};


/*DLLSHARED*/ int nk_msgbox(
    struct nkgdi_window*    parent,
    nk_msgbox_type          type,
    nk_msgbox_btn           btn,
    const std::string&      title,
    const std::string&      message
)
{
    int result = -1;
    
#ifdef NK_COMMDLG_USE_NATIVE 
    UINT flags = 0;

    /* set dlg icon */
    switch (type)
    {
    case nk_msgbox_type_info:
        flags |= MB_ICONINFORMATION;
        break;

    case nk_msgbox_type_warn:
        flags |= MB_ICONWARNING;
        break;

    case nk_msgbox_type_err:
        flags |= MB_ICONERROR;
        break;

    default:
        ASSERT(false, "unknown type for nk_msgbox = %d", type);
        break;
    };

    /* set dlg button set */
    switch (btn)
    {
    case nk_msgbox_btn_ok:
        flags |= MB_OK;
        break;

    case nk_msgbox_btn_okcancel:
        flags |= MB_OKCANCEL;
        break;

    default:
        ASSERT(false, "unknown btn type set for nk_msgbox = %d", btn);
        break;
    };

    /* call dlg and get result */
    result = MessageBoxW(nkgdi_window_get_native(parent), nku_utf8_to_wstr(message).c_str(), nku_utf8_to_wstr(title).c_str(), flags);
    switch (result)
    {
    case IDOK:
        result = 0;
        break;
        
    case IDCANCEL:
        result = 1;
        break;

    default:
        ASSERT(false, "unsupported return for nk_msgbox = %d", result);
        result = -1;
        break;
    };
#else
    nk_commdlg_ctx ctx(type, btn, title, message);

    nkgdi_window_show(
        "",
        760,
        480,
        -1,
        -1,
        nk_font_get_name(),
        nk_font_get_height(),
        nkgdi_window_flag_close,
        nk_commdlg_draw_proc,
        parent,
        &ctx
    );
#endif
    return result;
};