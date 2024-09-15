#include "AppUI.hpp"
#include "AppRc.hpp"
#include "AppModule.hpp"
#include "AppSettings.hpp"

#include "Shared/Common/Configuration.hpp"
#include "Shared/File/File.hpp"
#include "Shared/File/FsRc.hpp"
#include "Shared/UI/nk_text.hpp"
#include "Shared/UI/nk_font.hpp"
#include "Shared/UI/nk_window.hpp"


class CAppUI final
{
private:
    static const std::size_t invalid_mod_id = std::numeric_limits<std::size_t>::max();
    
    struct MOD_INFO
    {
        MOD_INFO(std::size_t id = invalid_mod_id);
        
        std::size_t m_id;
        HINSTANCE m_hInstance;
		int32 m_rcIcoId;
		std::string m_title;
		std::string m_desc;
		bool(*m_drawproc)(struct nkgdi_window*, struct nk_context*, bool);
	};

public:
    explicit CAppUI();
    ~CAppUI();
    void Loop();
    bool InitProc(struct nkgdi_window* wnd, struct nk_context* ctx, bool ret);
    bool DrawProc(struct nkgdi_window* wnd, struct nk_context* ctx, bool ret);
    
    inline const MOD_INFO& GetModuleInfo(std::size_t id)            { return (m_modinfo = MOD_INFO(id)); };
    inline cb_draw GetModuleDrawProc(std::size_t id)                { return GetModuleInfo(id).m_drawproc; };
    inline HINSTANCE GetModuleInstance(std::size_t id)              { return GetModuleInfo(id).m_hInstance; };
    inline const std::string& GetModuleTitle(std::size_t id)        { return GetModuleInfo(id).m_title; };
    inline const std::string& GetModuleDescription(std::size_t id)  { return GetModuleInfo(id).m_desc; };    
    inline int32 GetModuleIconId(std::size_t id)                    { return GetModuleInfo(id).m_rcIcoId; };

private:
    MOD_INFO m_modinfo;
    std::size_t m_modCount;
    std::vector<struct nk_image> m_modIcoArray;
};


CAppUI::MOD_INFO::MOD_INFO(std::size_t id /*= invalid_mod_id*/)
: m_id(invalid_mod_id)
, m_hInstance(NULL)
, m_rcIcoId(-1)
, m_title()
, m_desc()
, m_drawproc(nullptr)
{
	std::size_t idPrev = m_id;
	m_id = id;

    bool isValidModId = (m_id != invalid_mod_id);
    bool isSameModId = (m_id == idPrev);

    if (isValidModId && !isSameModId)
        AppModGetInfo(m_id, m_hInstance, m_rcIcoId, m_title, m_desc, m_drawproc);
};


CAppUI::CAppUI()
: m_modinfo()
, m_modCount(0u)
, m_modIcoArray()
{
    /* init text & mod manager */
    AppModInitialize();

    /* init icons */
    m_modCount = AppModGetCount();
    m_modIcoArray.resize(m_modCount);

	for (std::size_t i = 0; i < m_modCount; ++i)
	{
		if (GetModuleIconId(i) != -1)
			nk_img_create_mod_rc(&m_modIcoArray[i], GetModuleInstance(i), GetModuleIconId(i));
	};

    /* show main ui window */
	uint32 flags = 0;
	flags |= nkgdi_window_flag_layered;
	//flags |= nkgdi_window_flag_resize;
    flags |= nkgdi_window_flag_min;
    //flags |= nkgdi_window_flag_max;
    flags |= nkgdi_window_flag_close;
	flags |= nkgdi_window_flag_ico;
	flags |= nkgdi_window_flag_hide_return;

    nk_font_set_height(AppSettings.FontH);

    nkgdi_window_show(
        "",
        AppSettings.WindowW,
        AppSettings.WindowH,
        -1,
        -1,
        nk_font_get_name(),
        nk_font_get_height(),
        flags,
        nkgdi_window_obj_drawproc(this, &CAppUI::InitProc),
        nullptr,
        nullptr
    );
};


CAppUI::~CAppUI()
{
    /* destroy icons */
    std::for_each(m_modIcoArray.begin(), m_modIcoArray.end(), [](struct nk_image& img) {
        nk_img_destroy(&img);
    });

    /* shutdown mod man */
    AppModTerminate();
};


void CAppUI::Loop()
{
    nkgdi_window_loop();
};


bool CAppUI::InitProc(struct nkgdi_window* wnd, struct nk_context* ctx, bool ret)
{
    if (!ret)
    {
        nk_text_load_rc("app_en", IDF_APPTEXT_EN);
		nkgdi_window_set_draw_proc(wnd, nkgdi_window_obj_drawproc(this, &CAppUI::DrawProc));
    };

    return ret;
};


bool CAppUI::DrawProc(struct nkgdi_window* wnd, struct nk_context* ctx, bool ret)
{
    /* set window generic params */
    nk_text_path_locale_push("app");
    nkgdi_window_set_ico(wnd, nullptr);

    nkgdi_window_set_title(wnd, nk_text_id(1));
    nkgdi_window_set_min_size(wnd, 400, 200);

	uint32 flags = nkgdi_window_get_flags(wnd);
	flags |= (nkgdi_window_flag_hide_return);
	nkgdi_window_set_flags(wnd, flags);

	/* set button style */
    nk_style_push_float(ctx, &ctx->style.button.border, 0.0f);
    nk_style_push_color(ctx, &ctx->style.button.normal.data.color, nk_rgb(0, 0, 0));
    nk_style_push_color(ctx, &ctx->style.button.hover.data.color, nk_rgb(25, 25, 25));
	nk_style_push_color(ctx, &ctx->style.button.active.data.color, nk_rgb(53, 53, 53));

    /* draw module buttons or empty space */
    if (m_modCount > 0)
    {
        nk_layout_row_dynamic(ctx, 64, 1);

        nk_style_push_vec2(ctx, &ctx->style.button.image_padding, nk_vec2(12.0f, 12.0f));
        nk_style_push_float(ctx, &ctx->style.button.border, 0.0f);

        if (!ret)
            AppSettings.ModCurSel[0] = '\0';

        for (std::size_t i = 0; i < m_modCount; ++i)
		{
			bool bPress = false;

			if (CfgIsArgPresent("test"))
			{
				if (!m_modIcoArray[i].handle.ptr)
					bPress = true;
			}
			else
			{
				if (m_modIcoArray[i].handle.ptr)
					bPress = true;// nk_button_image_label(ctx, m_modIcoArray[i], GetModuleTitle(i).c_str(), NK_TEXT_CENTERED);
			};

			if (bPress && !ret)
            {
                AppModSetCurrent(i);
                nkgdi_window_push_draw_proc(wnd, GetModuleDrawProc(i));
				std::strcpy(AppSettings.ModCurSel, GetModuleTitle(i).c_str());
				if (m_modIcoArray[i].handle.ptr)
					nkgdi_window_set_ico(wnd, &m_modIcoArray[i]);
            };
        };

        nk_style_pop_float(ctx);
        nk_style_pop_vec2(ctx);
    }
    else
    {
        int32 ww = 0;
        int32 wh = 0;
        nkgdi_window_get_size(wnd, &ww, &wh);

        nk_layout_row_dynamic(ctx, float(wh), 3);

        nk_widget_disable_begin(ctx);
        nk_spacer(ctx);
        nk_label(ctx, nk_text_id(2), NK_TEXT_CENTERED);
        nk_spacer(ctx);
        nk_widget_disable_end(ctx);
    };

    /* restore button style */
    nk_style_pop_color(ctx);
    nk_style_pop_color(ctx);
    nk_style_pop_color(ctx);
    nk_style_pop_float(ctx);

    nk_text_path_pop();

    /* save winow settings */
    int32 ww = 0;
    int32 wh = 0;
    nkgdi_window_get_size(wnd, &ww, &wh);
    AppSettings.WindowW = ww;
    AppSettings.WindowH = wh;

    return false;
};


void AppUI_Loop()
{
    CAppUI().Loop();
};


bool AppUI_IsSettingsChange()
{
    return false;
};